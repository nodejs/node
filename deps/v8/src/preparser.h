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
#include "token.h"
#include "scanner.h"

namespace v8 {
namespace internal {

// Common base class shared between parser and pre-parser.
class ParserBase {
 public:
  ParserBase(Scanner* scanner, uintptr_t stack_limit)
      : scanner_(scanner),
        stack_limit_(stack_limit),
        stack_overflow_(false),
        allow_lazy_(false),
        allow_natives_syntax_(false),
        allow_generators_(false),
        allow_for_of_(false) { }
  // TODO(mstarzinger): Only virtual until message reporting has been unified.
  virtual ~ParserBase() { }

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

  Scanner* scanner() const { return scanner_; }
  int position() { return scanner_->location().beg_pos; }
  int peek_position() { return scanner_->peek_location().beg_pos; }
  bool stack_overflow() const { return stack_overflow_; }
  void set_stack_overflow() { stack_overflow_ = true; }

  virtual bool is_classic_mode() = 0;

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

  bool peek_any_identifier();
  void ExpectSemicolon(bool* ok);
  bool CheckContextualKeyword(Vector<const char> keyword);
  void ExpectContextualKeyword(Vector<const char> keyword, bool* ok);

  // Strict mode octal literal validation.
  void CheckOctalLiteral(int beg_pos, int end_pos, bool* ok);

  // Determine precedence of given token.
  static int Precedence(Token::Value token, bool accept_IN);

  // Report syntax errors.
  void ReportUnexpectedToken(Token::Value token);
  void ReportMessageAt(Scanner::Location location, const char* type) {
    ReportMessageAt(location, type, Vector<const char*>::empty());
  }
  virtual void ReportMessageAt(Scanner::Location source_location,
                               const char* message,
                               Vector<const char*> args) = 0;

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

 private:
  Scanner* scanner_;
  uintptr_t stack_limit_;
  bool stack_overflow_;

  bool allow_lazy_;
  bool allow_natives_syntax_;
  bool allow_generators_;
  bool allow_for_of_;
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
class PreParser : public ParserBase {
 public:
  enum PreParseResult {
    kPreParseStackOverflow,
    kPreParseSuccess
  };

  PreParser(Scanner* scanner,
            ParserRecorder* log,
            uintptr_t stack_limit)
      : ParserBase(scanner, stack_limit),
        log_(log),
        scope_(NULL),
        parenthesized_function_(false) { }

  ~PreParser() {}

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  PreParseResult PreParseProgram() {
    Scope top_scope(&scope_, kTopLevelScope);
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
  // These types form an algebra over syntactic categories that is just
  // rich enough to let us recognize and propagate the constructs that
  // are either being counted in the preparser data, or is important
  // to throw the correct syntax error exceptions.

  enum ScopeType {
    kTopLevelScope,
    kFunctionScope
  };

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

  class Expression;

  class Identifier {
   public:
    static Identifier Default() {
      return Identifier(kUnknownIdentifier);
    }
    static Identifier Eval()  {
      return Identifier(kEvalIdentifier);
    }
    static Identifier Arguments()  {
      return Identifier(kArgumentsIdentifier);
    }
    static Identifier FutureReserved()  {
      return Identifier(kFutureReservedIdentifier);
    }
    static Identifier FutureStrictReserved()  {
      return Identifier(kFutureStrictReservedIdentifier);
    }
    static Identifier Yield()  {
      return Identifier(kYieldIdentifier);
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
    explicit Identifier(Type type) : type_(type) { }
    Type type_;

    friend class Expression;
  };

  // Bits 0 and 1 are used to identify the type of expression:
  // If bit 0 is set, it's an identifier.
  // if bit 1 is set, it's a string literal.
  // If neither is set, it's no particular type, and both set isn't
  // use yet.
  class Expression {
   public:
    static Expression Default() {
      return Expression(kUnknownExpression);
    }

    static Expression FromIdentifier(Identifier id) {
      return Expression(kIdentifierFlag | (id.type_ << kIdentifierShift));
    }

    static Expression StringLiteral() {
      return Expression(kUnknownStringLiteral);
    }

    static Expression UseStrictStringLiteral() {
      return Expression(kUseStrictString);
    }

    static Expression This() {
      return Expression(kThisExpression);
    }

    static Expression ThisProperty() {
      return Expression(kThisPropertyExpression);
    }

    static Expression StrictFunction() {
      return Expression(kStrictFunctionExpression);
    }

    bool IsIdentifier() {
      return (code_ & kIdentifierFlag) != 0;
    }

    // Only works corretly if it is actually an identifier expression.
    PreParser::Identifier AsIdentifier() {
      return PreParser::Identifier(
          static_cast<PreParser::Identifier::Type>(code_ >> kIdentifierShift));
    }

    bool IsStringLiteral() { return (code_ & kStringLiteralFlag) != 0; }

    bool IsUseStrictLiteral() {
      return (code_ & kStringLiteralMask) == kUseStrictString;
    }

    bool IsThis() {
      return code_ == kThisExpression;
    }

    bool IsThisProperty() {
      return code_ == kThisPropertyExpression;
    }

    bool IsStrictFunction() {
      return code_ == kStrictFunctionExpression;
    }

   private:
    // First two/three bits are used as flags.
    // Bit 0 and 1 represent identifiers or strings literals, and are
    // mutually exclusive, but can both be absent.
    enum  {
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

    explicit Expression(int expression_code) : code_(expression_code) { }

    int code_;
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

  class Scope {
   public:
    Scope(Scope** variable, ScopeType type)
        : variable_(variable),
          prev_(*variable),
          type_(type),
          materialized_literal_count_(0),
          expected_properties_(0),
          with_nesting_count_(0),
          language_mode_(
              (prev_ != NULL) ? prev_->language_mode() : CLASSIC_MODE),
          is_generator_(false) {
      *variable = this;
    }
    ~Scope() { *variable_ = prev_; }
    void NextMaterializedLiteralIndex() { materialized_literal_count_++; }
    void AddProperty() { expected_properties_++; }
    ScopeType type() { return type_; }
    int expected_properties() { return expected_properties_; }
    int materialized_literal_count() { return materialized_literal_count_; }
    bool IsInsideWith() { return with_nesting_count_ != 0; }
    bool is_generator() { return is_generator_; }
    void set_is_generator(bool is_generator) { is_generator_ = is_generator; }
    bool is_classic_mode() {
      return language_mode_ == CLASSIC_MODE;
    }
    LanguageMode language_mode() {
      return language_mode_;
    }
    void set_language_mode(LanguageMode language_mode) {
      language_mode_ = language_mode;
    }

    class InsideWith {
     public:
      explicit InsideWith(Scope* scope) : scope_(scope) {
        scope->with_nesting_count_++;
      }

      ~InsideWith() { scope_->with_nesting_count_--; }

     private:
      Scope* scope_;
      DISALLOW_COPY_AND_ASSIGN(InsideWith);
    };

   private:
    Scope** const variable_;
    Scope* const prev_;
    const ScopeType type_;
    int materialized_literal_count_;
    int expected_properties_;
    int with_nesting_count_;
    LanguageMode language_mode_;
    bool is_generator_;
  };

  // Report syntax error
  void ReportMessageAt(Scanner::Location location,
                       const char* message,
                       Vector<const char*> args) {
    ReportMessageAt(location.beg_pos,
                    location.end_pos,
                    message,
                    args.length() > 0 ? args[0] : NULL);
  }
  void ReportMessageAt(Scanner::Location location,
                       const char* type,
                       const char* name_opt) {
    log_->LogMessage(location.beg_pos, location.end_pos, type, name_opt);
  }
  void ReportMessageAt(int start_pos,
                       int end_pos,
                       const char* type,
                       const char* name_opt) {
    log_->LogMessage(start_pos, end_pos, type, name_opt);
  }

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
  Expression ParseNewExpression(bool* ok);
  Expression ParseMemberExpression(bool* ok);
  Expression ParseMemberWithNewPrefixesExpression(unsigned new_count, bool* ok);
  Expression ParsePrimaryExpression(bool* ok);
  Expression ParseArrayLiteral(bool* ok);
  Expression ParseObjectLiteral(bool* ok);
  Expression ParseRegExpLiteral(bool seen_equal, bool* ok);
  Expression ParseV8Intrinsic(bool* ok);

  Arguments ParseArguments(bool* ok);
  Expression ParseFunctionLiteral(
      Identifier name,
      Scanner::Location function_name_location,
      bool name_is_strict_reserved,
      bool is_generator,
      bool* ok);
  void ParseLazyFunctionLiteralBody(bool* ok);

  Identifier ParseIdentifier(AllowEvalOrArgumentsAsIdentifier, bool* ok);
  Identifier ParseIdentifierOrStrictReservedWord(bool* is_strict_reserved,
                                                 bool* ok);
  Identifier ParseIdentifierName(bool* ok);
  Identifier ParseIdentifierNameOrGetOrSet(bool* is_get,
                                           bool* is_set,
                                           bool* ok);

  // Logs the currently parsed literal as a symbol in the preparser data.
  void LogSymbol();
  // Log the currently parsed identifier.
  Identifier GetIdentifierSymbol();
  // Log the currently parsed string literal.
  Expression GetStringSymbol();

  void set_language_mode(LanguageMode language_mode) {
    scope_->set_language_mode(language_mode);
  }

  virtual bool is_classic_mode() {
    return scope_->language_mode() == CLASSIC_MODE;
  }

  bool is_extended_mode() {
    return scope_->language_mode() == EXTENDED_MODE;
  }

  LanguageMode language_mode() { return scope_->language_mode(); }

  bool CheckInOrOf(bool accept_OF);

  ParserRecorder* log_;
  Scope* scope_;
  bool parenthesized_function_;
};

} }  // v8::internal

#endif  // V8_PREPARSER_H
