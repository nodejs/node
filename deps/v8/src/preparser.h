// Copyright 2010 the V8 project authors. All rights reserved.
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

namespace v8 {
namespace preparser {

// Preparsing checks a JavaScript program and emits preparse-data that helps
// a later parsing to be faster.
// See preparse-data.h for the data.

// The PreParser checks that the syntax follows the grammar for JavaScript,
// and collects some information about the program along the way.
// The grammar check is only performed in order to understand the program
// sufficiently to deduce some information about it, that can be used
// to speed up later parsing. Finding errors is not the goal of pre-parsing,
// rather it is to speed up properly written and correct programs.
// That means that contextual checks (like a label being declared where
// it is used) are generally omitted.

namespace i = v8::internal;

enum StatementType {
  kUnknownStatement
};

enum ExpressionType {
  kUnknownExpression,
  kIdentifierExpression,  // Used to detect labels.
  kThisExpression,
  kThisPropertyExpression
};

enum IdentifierType {
  kUnknownIdentifier
};

enum SourceElementTypes {
  kUnknownSourceElements
};


typedef int SourceElements;
typedef int Expression;
typedef int Statement;
typedef int Identifier;
typedef int Arguments;


class PreParser {
 public:
  PreParser() : scope_(NULL), allow_lazy_(true) { }
  ~PreParser() { }

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  bool PreParseProgram(i::JavaScriptScanner* scanner,
                       i::ParserRecorder* log,
                       bool allow_lazy) {
    allow_lazy_ = allow_lazy;
    scanner_ = scanner;
    log_ = log;
    Scope top_scope(&scope_, kTopLevelScope);
    bool ok = true;
    ParseSourceElements(i::Token::EOS, &ok);
    bool stack_overflow = scanner_->stack_overflow();
    if (!ok && !stack_overflow) {
      ReportUnexpectedToken(scanner_->current_token());
    }
    return !stack_overflow;
  }

 private:
  enum ScopeType {
    kTopLevelScope,
    kFunctionScope
  };

  class Scope {
   public:
    Scope(Scope** variable, ScopeType type)
        : variable_(variable),
          prev_(*variable),
          type_(type),
          materialized_literal_count_(0),
          expected_properties_(0),
          with_nesting_count_(0) {
      *variable = this;
    }
    ~Scope() { *variable_ = prev_; }
    void NextMaterializedLiteralIndex() { materialized_literal_count_++; }
    void AddProperty() { expected_properties_++; }
    ScopeType type() { return type_; }
    int expected_properties() { return expected_properties_; }
    int materialized_literal_count() { return materialized_literal_count_; }
    bool IsInsideWith() { return with_nesting_count_ != 0; }
    void EnterWith() { with_nesting_count_++; }
    void LeaveWith() { with_nesting_count_--; }

   private:
    Scope** const variable_;
    Scope* const prev_;
    const ScopeType type_;
    int materialized_literal_count_;
    int expected_properties_;
    int with_nesting_count_;
  };

  // Types that allow us to recognize simple this-property assignments.
  // A simple this-property assignment is a statement on the form
  // "this.propertyName = {primitive constant or function parameter name);"
  // where propertyName isn't "__proto__".
  // The result is only relevant if the function body contains only
  // simple this-property assignments.

  // Report syntax error
  void ReportUnexpectedToken(i::Token::Value token);
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
  SourceElements ParseSourceElements(int end_token, bool* ok);
  Statement ParseStatement(bool* ok);
  Statement ParseFunctionDeclaration(bool* ok);
  Statement ParseNativeDeclaration(bool* ok);
  Statement ParseBlock(bool* ok);
  Statement ParseVariableStatement(bool* ok);
  Statement ParseVariableDeclarations(bool accept_IN, int* num_decl, bool* ok);
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

  Identifier ParseIdentifier(bool* ok);
  Identifier ParseIdentifierName(bool* ok);
  Identifier ParseIdentifierOrGetOrSet(bool* is_get, bool* is_set, bool* ok);

  Identifier GetIdentifierSymbol();
  unsigned int HexDigitValue(char digit);
  Expression GetStringSymbol();


  i::Token::Value peek() { return scanner_->peek(); }
  i::Token::Value Next() {
    i::Token::Value next = scanner_->Next();
    return next;
  }

  void Consume(i::Token::Value token) {
    Next();
  }

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

  i::JavaScriptScanner* scanner_;
  i::ParserRecorder* log_;
  Scope* scope_;
  bool allow_lazy_;
};
} }  // v8::preparser

#endif  // V8_PREPARSER_H
