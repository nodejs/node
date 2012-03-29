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
class UnicodeCache;
}

namespace preparser {

typedef uint8_t byte;

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

namespace i = v8::internal;

class DuplicateFinder {
 public:
  explicit DuplicateFinder(i::UnicodeCache* constants)
      : unicode_constants_(constants),
        backing_store_(16),
        map_(&Match) { }

  int AddAsciiSymbol(i::Vector<const char> key, int value);
  int AddUtf16Symbol(i::Vector<const uint16_t> key, int value);
  // Add a a number literal by converting it (if necessary)
  // to the string that ToString(ToNumber(literal)) would generate.
  // and then adding that string with AddAsciiSymbol.
  // This string is the actual value used as key in an object literal,
  // and the one that must be different from the other keys.
  int AddNumber(i::Vector<const char> key, int value);

 private:
  int AddSymbol(i::Vector<const byte> key, bool is_ascii, int value);
  // Backs up the key and its length in the backing store.
  // The backup is stored with a base 127 encoding of the
  // length (plus a bit saying whether the string is ASCII),
  // followed by the bytes of the key.
  byte* BackupKey(i::Vector<const byte> key, bool is_ascii);

  // Compare two encoded keys (both pointing into the backing store)
  // for having the same base-127 encoded lengths and ASCII-ness,
  // and then having the same 'length' bytes following.
  static bool Match(void* first, void* second);
  // Creates a hash from a sequence of bytes.
  static uint32_t Hash(i::Vector<const byte> key, bool is_ascii);
  // Checks whether a string containing a JS number is its canonical
  // form.
  static bool IsNumberCanonical(i::Vector<const char> key);

  // Size of buffer. Sufficient for using it to call DoubleToCString in
  // from conversions.h.
  static const int kBufferSize = 100;

  i::UnicodeCache* unicode_constants_;
  // Backing store used to store strings used as hashmap keys.
  i::SequenceCollector<unsigned char> backing_store_;
  i::HashMap map_;
  // Buffer used for string->number->canonical string conversions.
  char number_buffer_[kBufferSize];
};


class PreParser {
 public:
  enum PreParseResult {
    kPreParseStackOverflow,
    kPreParseSuccess
  };


  PreParser(i::Scanner* scanner,
            i::ParserRecorder* log,
            uintptr_t stack_limit,
            bool allow_lazy,
            bool allow_natives_syntax,
            bool allow_modules)
      : scanner_(scanner),
        log_(log),
        scope_(NULL),
        stack_limit_(stack_limit),
        strict_mode_violation_location_(i::Scanner::Location::invalid()),
        strict_mode_violation_type_(NULL),
        stack_overflow_(false),
        allow_lazy_(allow_lazy),
        allow_modules_(allow_modules),
        allow_natives_syntax_(allow_natives_syntax),
        parenthesized_function_(false),
        harmony_scoping_(scanner->HarmonyScoping()) { }

  ~PreParser() {}

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  static PreParseResult PreParseProgram(i::Scanner* scanner,
                                        i::ParserRecorder* log,
                                        int flags,
                                        uintptr_t stack_limit) {
    bool allow_lazy = (flags & i::kAllowLazy) != 0;
    bool allow_natives_syntax = (flags & i::kAllowNativesSyntax) != 0;
    bool allow_modules = (flags & i::kAllowModules) != 0;
    return PreParser(scanner, log, stack_limit, allow_lazy,
                     allow_natives_syntax, allow_modules).PreParse();
  }

  // Parses a single function literal, from the opening parentheses before
  // parameters to the closing brace after the body.
  // Returns a FunctionEntry describing the body of the funciton in enough
  // detail that it can be lazily compiled.
  // The scanner is expected to have matched the "function" keyword and
  // parameters, and have consumed the initial '{'.
  // At return, unless an error occured, the scanner is positioned before the
  // the final '}'.
  PreParseResult PreParseLazyFunction(i::LanguageMode mode,
                                      i::ParserRecorder* log);

 private:
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
  enum PropertyType {
    kNone = 0,
    // Bit patterns representing different object literal property types.
    kGetterProperty = 1,
    kSetterProperty = 2,
    kValueProperty = 7,
    // Helper constants.
    kValueFlag = 4
  };

  // Checks the type of conflict based on values coming from PropertyType.
  bool HasConflict(int type1, int type2) { return (type1 & type2) != 0; }
  bool IsDataDataConflict(int type1, int type2) {
    return ((type1 & type2) & kValueFlag) != 0;
  }
  bool IsDataAccessorConflict(int type1, int type2) {
    return ((type1 ^ type2) & kValueFlag) != 0;
  }
  bool IsAccessorAccessorConflict(int type1, int type2) {
    return ((type1 | type2) & kValueFlag) == 0;
  }


  void CheckDuplicate(DuplicateFinder* finder,
                      i::Token::Value property,
                      int type,
                      bool* ok);

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
    bool IsEval() { return type_ == kEvalIdentifier; }
    bool IsArguments() { return type_ == kArgumentsIdentifier; }
    bool IsEvalOrArguments() { return type_ >= kEvalIdentifier; }
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
  // Bit 2 is used to mark the expression as being parenthesized,
  // so "(foo)" isn't recognized as a pure identifier (and possible label).
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

    bool IsParenthesized() {
      // If bit 0 or 1 is set, we interpret bit 2 as meaning parenthesized.
      return (code_ & 7) > 4;
    }

    bool IsRawIdentifier() {
      return !IsParenthesized() && IsIdentifier();
    }

    bool IsStringLiteral() { return (code_ & kStringLiteralFlag) != 0; }

    bool IsRawStringLiteral() {
      return !IsParenthesized() && IsStringLiteral();
    }

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

    Expression Parenthesize() {
      int type = code_ & 3;
      if (type != 0) {
        // Identifiers and string literals can be parenthesized.
        // They no longer work as labels or directive prologues,
        // but are still recognized in other contexts.
        return Expression(code_ | kParentesizedExpressionFlag);
      }
      // For other types of expressions, it's not important to remember
      // the parentheses.
      return *this;
    }

   private:
    // First two/three bits are used as flags.
    // Bit 0 and 1 represent identifiers or strings literals, and are
    // mutually exclusive, but can both be absent.
    // If bit 0 or 1 are set, bit 2 marks that the expression has
    // been wrapped in parentheses (a string literal can no longer
    // be a directive prologue, and an identifier can no longer be
    // a label.
    enum  {
      kUnknownExpression = 0,
      // Identifiers
      kIdentifierFlag = 1,  // Used to detect labels.
      kIdentifierShift = 3,

      kStringLiteralFlag = 2,  // Used to detect directive prologue.
      kUnknownStringLiteral = kStringLiteralFlag,
      kUseStrictString = kStringLiteralFlag | 8,
      kStringLiteralMask = kUseStrictString,

      kParentesizedExpressionFlag = 4,  // Only if identifier or string literal.

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
      if (!expression.IsParenthesized()) {
        if (expression.IsUseStrictLiteral()) {
          return Statement(kUseStrictExpressionStatement);
        }
        if (expression.IsStringLiteral()) {
          return Statement(kStringLiteralExpressionStatement);
        }
      }
      return Default();
    }

    bool IsStringLiteral() {
      return code_ != kUnknownStatement;
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
              (prev_ != NULL) ? prev_->language_mode() : i::CLASSIC_MODE) {
      *variable = this;
    }
    ~Scope() { *variable_ = prev_; }
    void NextMaterializedLiteralIndex() { materialized_literal_count_++; }
    void AddProperty() { expected_properties_++; }
    ScopeType type() { return type_; }
    int expected_properties() { return expected_properties_; }
    int materialized_literal_count() { return materialized_literal_count_; }
    bool IsInsideWith() { return with_nesting_count_ != 0; }
    bool is_classic_mode() {
      return language_mode_ == i::CLASSIC_MODE;
    }
    i::LanguageMode language_mode() {
      return language_mode_;
    }
    void set_language_mode(i::LanguageMode language_mode) {
      language_mode_ = language_mode;
    }
    void EnterWith() { with_nesting_count_++; }
    void LeaveWith() { with_nesting_count_--; }

   private:
    Scope** const variable_;
    Scope* const prev_;
    const ScopeType type_;
    int materialized_literal_count_;
    int expected_properties_;
    int with_nesting_count_;
    i::LanguageMode language_mode_;
  };

  // Preparse the program. Only called in PreParseProgram after creating
  // the instance.
  PreParseResult PreParse() {
    Scope top_scope(&scope_, kTopLevelScope);
    bool ok = true;
    int start_position = scanner_->peek_location().beg_pos;
    ParseSourceElements(i::Token::EOS, &ok);
    if (stack_overflow_) return kPreParseStackOverflow;
    if (!ok) {
      ReportUnexpectedToken(scanner_->current_token());
    } else if (!scope_->is_classic_mode()) {
      CheckOctalLiteral(start_position, scanner_->location().end_pos, &ok);
    }
    return kPreParseSuccess;
  }

  // Report syntax error
  void ReportUnexpectedToken(i::Token::Value token);
  void ReportMessageAt(i::Scanner::Location location,
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

  void CheckOctalLiteral(int beg_pos, int end_pos, bool* ok);

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
  Expression ParseFunctionLiteral(bool* ok);
  void ParseLazyFunctionLiteralBody(bool* ok);

  Identifier ParseIdentifier(bool* ok);
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

  i::Token::Value peek() {
    if (stack_overflow_) return i::Token::ILLEGAL;
    return scanner_->peek();
  }

  i::Token::Value Next() {
    if (stack_overflow_) return i::Token::ILLEGAL;
    {
      int marker;
      if (reinterpret_cast<uintptr_t>(&marker) < stack_limit_) {
        // Further calls to peek/Next will return illegal token.
        // The current one will still be returned. It might already
        // have been seen using peek.
        stack_overflow_ = true;
      }
    }
    return scanner_->Next();
  }

  bool peek_any_identifier();

  void set_language_mode(i::LanguageMode language_mode) {
    scope_->set_language_mode(language_mode);
  }

  bool is_classic_mode() {
    return scope_->language_mode() == i::CLASSIC_MODE;
  }

  bool is_extended_mode() {
    return scope_->language_mode() == i::EXTENDED_MODE;
  }

  i::LanguageMode language_mode() { return scope_->language_mode(); }

  void Consume(i::Token::Value token) { Next(); }

  void Expect(i::Token::Value token, bool* ok) {
    if (Next() != token) {
      *ok = false;
    }
  }

  bool Check(i::Token::Value token) {
    i::Token::Value next = peek();
    if (next == token) {
      Consume(next);
      return true;
    }
    return false;
  }
  void ExpectSemicolon(bool* ok);

  static int Precedence(i::Token::Value tok, bool accept_IN);

  void SetStrictModeViolation(i::Scanner::Location,
                              const char* type,
                              bool* ok);

  void CheckDelayedStrictModeViolation(int beg_pos, int end_pos, bool* ok);

  void StrictModeIdentifierViolation(i::Scanner::Location,
                                     const char* eval_args_type,
                                     Identifier identifier,
                                     bool* ok);

  i::Scanner* scanner_;
  i::ParserRecorder* log_;
  Scope* scope_;
  uintptr_t stack_limit_;
  i::Scanner::Location strict_mode_violation_location_;
  const char* strict_mode_violation_type_;
  bool stack_overflow_;
  bool allow_lazy_;
  bool allow_modules_;
  bool allow_natives_syntax_;
  bool parenthesized_function_;
  bool harmony_scoping_;
};
} }  // v8::preparser

#endif  // V8_PREPARSER_H
