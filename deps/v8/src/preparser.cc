// Copyright 2011 the V8 project authors. All rights reserved.
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

#include <cmath>

#include "../include/v8stdint.h"

#include "allocation.h"
#include "checks.h"
#include "conversions.h"
#include "conversions-inl.h"
#include "globals.h"
#include "hashmap.h"
#include "list.h"
#include "preparse-data-format.h"
#include "preparse-data.h"
#include "preparser.h"
#include "unicode.h"
#include "utils.h"

#ifdef _MSC_VER
namespace std {

// Usually defined in math.h, but not in MSVC.
// Abstracted to work
int isfinite(double value);

}  // namespace std
#endif

namespace v8 {

namespace preparser {

PreParser::PreParseResult PreParser::PreParseLazyFunction(
    i::LanguageMode mode, bool is_generator, i::ParserRecorder* log) {
  log_ = log;
  // Lazy functions always have trivial outer scopes (no with/catch scopes).
  Scope top_scope(&scope_, kTopLevelScope);
  set_language_mode(mode);
  Scope function_scope(&scope_, kFunctionScope);
  function_scope.set_is_generator(is_generator);
  ASSERT_EQ(i::Token::LBRACE, scanner_->current_token());
  bool ok = true;
  int start_position = scanner_->peek_location().beg_pos;
  ParseLazyFunctionLiteralBody(&ok);
  if (stack_overflow_) return kPreParseStackOverflow;
  if (!ok) {
    ReportUnexpectedToken(scanner_->current_token());
  } else {
    ASSERT_EQ(i::Token::RBRACE, scanner_->peek());
    if (!is_classic_mode()) {
      int end_pos = scanner_->location().end_pos;
      CheckOctalLiteral(start_position, end_pos, &ok);
      if (ok) {
        CheckDelayedStrictModeViolation(start_position, end_pos, &ok);
      }
    }
  }
  return kPreParseSuccess;
}


// Preparsing checks a JavaScript program and emits preparse-data that helps
// a later parsing to be faster.
// See preparser-data.h for the data.

// The PreParser checks that the syntax follows the grammar for JavaScript,
// and collects some information about the program along the way.
// The grammar check is only performed in order to understand the program
// sufficiently to deduce some information about it, that can be used
// to speed up later parsing. Finding errors is not the goal of pre-parsing,
// rather it is to speed up properly written and correct programs.
// That means that contextual checks (like a label being declared where
// it is used) are generally omitted.

void PreParser::ReportUnexpectedToken(i::Token::Value token) {
  // We don't report stack overflows here, to avoid increasing the
  // stack depth even further.  Instead we report it after parsing is
  // over, in ParseProgram.
  if (token == i::Token::ILLEGAL && stack_overflow_) {
    return;
  }
  i::Scanner::Location source_location = scanner_->location();

  // Four of the tokens are treated specially
  switch (token) {
  case i::Token::EOS:
    return ReportMessageAt(source_location, "unexpected_eos", NULL);
  case i::Token::NUMBER:
    return ReportMessageAt(source_location, "unexpected_token_number", NULL);
  case i::Token::STRING:
    return ReportMessageAt(source_location, "unexpected_token_string", NULL);
  case i::Token::IDENTIFIER:
    return ReportMessageAt(source_location,
                           "unexpected_token_identifier", NULL);
  case i::Token::FUTURE_RESERVED_WORD:
    return ReportMessageAt(source_location, "unexpected_reserved", NULL);
  case i::Token::FUTURE_STRICT_RESERVED_WORD:
    return ReportMessageAt(source_location,
                           "unexpected_strict_reserved", NULL);
  default:
    const char* name = i::Token::String(token);
    ReportMessageAt(source_location, "unexpected_token", name);
  }
}


// Checks whether octal literal last seen is between beg_pos and end_pos.
// If so, reports an error.
void PreParser::CheckOctalLiteral(int beg_pos, int end_pos, bool* ok) {
  i::Scanner::Location octal = scanner_->octal_position();
  if (beg_pos <= octal.beg_pos && octal.end_pos <= end_pos) {
    ReportMessageAt(octal, "strict_octal_literal", NULL);
    scanner_->clear_octal_position();
    *ok = false;
  }
}


#define CHECK_OK  ok);                      \
  if (!*ok) return kUnknownSourceElements;  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY


PreParser::Statement PreParser::ParseSourceElement(bool* ok) {
  // (Ecma 262 5th Edition, clause 14):
  // SourceElement:
  //    Statement
  //    FunctionDeclaration
  //
  // In harmony mode we allow additionally the following productions
  // SourceElement:
  //    LetDeclaration
  //    ConstDeclaration
  //    GeneratorDeclaration

  switch (peek()) {
    case i::Token::FUNCTION:
      return ParseFunctionDeclaration(ok);
    case i::Token::LET:
    case i::Token::CONST:
      return ParseVariableStatement(kSourceElement, ok);
    default:
      return ParseStatement(ok);
  }
}


PreParser::SourceElements PreParser::ParseSourceElements(int end_token,
                                                         bool* ok) {
  // SourceElements ::
  //   (Statement)* <end_token>

  bool allow_directive_prologue = true;
  while (peek() != end_token) {
    Statement statement = ParseSourceElement(CHECK_OK);
    if (allow_directive_prologue) {
      if (statement.IsUseStrictLiteral()) {
        set_language_mode(allow_harmony_scoping() ?
                          i::EXTENDED_MODE : i::STRICT_MODE);
      } else if (!statement.IsStringLiteral()) {
        allow_directive_prologue = false;
      }
    }
  }
  return kUnknownSourceElements;
}


#undef CHECK_OK
#define CHECK_OK  ok);                   \
  if (!*ok) return Statement::Default();  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY


PreParser::Statement PreParser::ParseStatement(bool* ok) {
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

  // Keep the source position of the statement
  switch (peek()) {
    case i::Token::LBRACE:
      return ParseBlock(ok);

    case i::Token::CONST:
    case i::Token::LET:
    case i::Token::VAR:
      return ParseVariableStatement(kStatement, ok);

    case i::Token::SEMICOLON:
      Next();
      return Statement::Default();

    case i::Token::IF:
      return ParseIfStatement(ok);

    case i::Token::DO:
      return ParseDoWhileStatement(ok);

    case i::Token::WHILE:
      return ParseWhileStatement(ok);

    case i::Token::FOR:
      return ParseForStatement(ok);

    case i::Token::CONTINUE:
      return ParseContinueStatement(ok);

    case i::Token::BREAK:
      return ParseBreakStatement(ok);

    case i::Token::RETURN:
      return ParseReturnStatement(ok);

    case i::Token::WITH:
      return ParseWithStatement(ok);

    case i::Token::SWITCH:
      return ParseSwitchStatement(ok);

    case i::Token::THROW:
      return ParseThrowStatement(ok);

    case i::Token::TRY:
      return ParseTryStatement(ok);

    case i::Token::FUNCTION: {
      i::Scanner::Location start_location = scanner_->peek_location();
      Statement statement = ParseFunctionDeclaration(CHECK_OK);
      i::Scanner::Location end_location = scanner_->location();
      if (!is_classic_mode()) {
        ReportMessageAt(start_location.beg_pos, end_location.end_pos,
                        "strict_function", NULL);
        *ok = false;
        return Statement::Default();
      } else {
        return statement;
      }
    }

    case i::Token::DEBUGGER:
      return ParseDebuggerStatement(ok);

    default:
      return ParseExpressionOrLabelledStatement(ok);
  }
}


PreParser::Statement PreParser::ParseFunctionDeclaration(bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameterListopt ')' '{' FunctionBody '}'
  // GeneratorDeclaration ::
  //   'function' '*' Identifier '(' FormalParameterListopt ')'
  //      '{' FunctionBody '}'
  Expect(i::Token::FUNCTION, CHECK_OK);

  bool is_generator = allow_generators_ && Check(i::Token::MUL);
  Identifier identifier = ParseIdentifier(CHECK_OK);
  i::Scanner::Location location = scanner_->location();

  Expression function_value = ParseFunctionLiteral(is_generator, CHECK_OK);

  if (function_value.IsStrictFunction() &&
      !identifier.IsValidStrictVariable()) {
    // Strict mode violation, using either reserved word or eval/arguments
    // as name of strict function.
    const char* type = "strict_function_name";
    if (identifier.IsFutureStrictReserved() || identifier.IsYield()) {
      type = "strict_reserved_word";
    }
    ReportMessageAt(location, type, NULL);
    *ok = false;
  }
  return Statement::FunctionDeclaration();
}


PreParser::Statement PreParser::ParseBlock(bool* ok) {
  // Block ::
  //   '{' Statement* '}'

  // Note that a Block does not introduce a new execution scope!
  // (ECMA-262, 3rd, 12.2)
  //
  Expect(i::Token::LBRACE, CHECK_OK);
  while (peek() != i::Token::RBRACE) {
    if (is_extended_mode()) {
      ParseSourceElement(CHECK_OK);
    } else {
      ParseStatement(CHECK_OK);
    }
  }
  Expect(i::Token::RBRACE, ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseVariableStatement(
    VariableDeclarationContext var_context,
    bool* ok) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  Statement result = ParseVariableDeclarations(var_context,
                                               NULL,
                                               NULL,
                                               CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return result;
}


// If the variable declaration declares exactly one non-const
// variable, then *var is set to that variable. In all other cases,
// *var is untouched; in particular, it is the caller's responsibility
// to initialize it properly. This mechanism is also used for the parsing
// of 'for-in' loops.
PreParser::Statement PreParser::ParseVariableDeclarations(
    VariableDeclarationContext var_context,
    VariableDeclarationProperties* decl_props,
    int* num_decl,
    bool* ok) {
  // VariableDeclarations ::
  //   ('var' | 'const') (Identifier ('=' AssignmentExpression)?)+[',']
  //
  // The ES6 Draft Rev3 specifies the following grammar for const declarations
  //
  // ConstDeclaration ::
  //   const ConstBinding (',' ConstBinding)* ';'
  // ConstBinding ::
  //   Identifier '=' AssignmentExpression
  //
  // TODO(ES6):
  // ConstBinding ::
  //   BindingPattern '=' AssignmentExpression
  bool require_initializer = false;
  if (peek() == i::Token::VAR) {
    Consume(i::Token::VAR);
  } else if (peek() == i::Token::CONST) {
    // TODO(ES6): The ES6 Draft Rev4 section 12.2.2 reads:
    //
    // ConstDeclaration : const ConstBinding (',' ConstBinding)* ';'
    //
    // * It is a Syntax Error if the code that matches this production is not
    //   contained in extended code.
    //
    // However disallowing const in classic mode will break compatibility with
    // existing pages. Therefore we keep allowing const with the old
    // non-harmony semantics in classic mode.
    Consume(i::Token::CONST);
    switch (language_mode()) {
      case i::CLASSIC_MODE:
        break;
      case i::STRICT_MODE: {
        i::Scanner::Location location = scanner_->peek_location();
        ReportMessageAt(location, "strict_const", NULL);
        *ok = false;
        return Statement::Default();
      }
      case i::EXTENDED_MODE:
        if (var_context != kSourceElement &&
            var_context != kForStatement) {
          i::Scanner::Location location = scanner_->peek_location();
          ReportMessageAt(location.beg_pos, location.end_pos,
                          "unprotected_const", NULL);
          *ok = false;
          return Statement::Default();
        }
        require_initializer = true;
        break;
    }
  } else if (peek() == i::Token::LET) {
    // ES6 Draft Rev4 section 12.2.1:
    //
    // LetDeclaration : let LetBindingList ;
    //
    // * It is a Syntax Error if the code that matches this production is not
    //   contained in extended code.
    if (!is_extended_mode()) {
      i::Scanner::Location location = scanner_->peek_location();
      ReportMessageAt(location.beg_pos, location.end_pos,
                      "illegal_let", NULL);
      *ok = false;
      return Statement::Default();
    }
    Consume(i::Token::LET);
    if (var_context != kSourceElement &&
        var_context != kForStatement) {
      i::Scanner::Location location = scanner_->peek_location();
      ReportMessageAt(location.beg_pos, location.end_pos,
                      "unprotected_let", NULL);
      *ok = false;
      return Statement::Default();
    }
  } else {
    *ok = false;
    return Statement::Default();
  }

  // The scope of a var/const declared variable anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). The scope
  // of a let declared variable is the scope of the immediately enclosing
  // block.
  int nvars = 0;  // the number of variables declared
  do {
    // Parse variable name.
    if (nvars > 0) Consume(i::Token::COMMA);
    Identifier identifier  = ParseIdentifier(CHECK_OK);
    if (!is_classic_mode() && !identifier.IsValidStrictVariable()) {
      StrictModeIdentifierViolation(scanner_->location(),
                                    "strict_var_name",
                                    identifier,
                                    ok);
      return Statement::Default();
    }
    nvars++;
    if (peek() == i::Token::ASSIGN || require_initializer) {
      Expect(i::Token::ASSIGN, CHECK_OK);
      ParseAssignmentExpression(var_context != kForStatement, CHECK_OK);
      if (decl_props != NULL) *decl_props = kHasInitializers;
    }
  } while (peek() == i::Token::COMMA);

  if (num_decl != NULL) *num_decl = nvars;
  return Statement::Default();
}


PreParser::Statement PreParser::ParseExpressionOrLabelledStatement(bool* ok) {
  // ExpressionStatement | LabelledStatement ::
  //   Expression ';'
  //   Identifier ':' Statement

  Expression expr = ParseExpression(true, CHECK_OK);
  if (expr.IsRawIdentifier()) {
    ASSERT(!expr.AsIdentifier().IsFutureReserved());
    ASSERT(is_classic_mode() ||
           (!expr.AsIdentifier().IsFutureStrictReserved() &&
            !expr.AsIdentifier().IsYield()));
    if (peek() == i::Token::COLON) {
      Consume(i::Token::COLON);
      return ParseStatement(ok);
    }
    // Preparsing is disabled for extensions (because the extension details
    // aren't passed to lazily compiled functions), so we don't
    // accept "native function" in the preparser.
  }
  // Parsed expression statement.
  ExpectSemicolon(CHECK_OK);
  return Statement::ExpressionStatement(expr);
}


PreParser::Statement PreParser::ParseIfStatement(bool* ok) {
  // IfStatement ::
  //   'if' '(' Expression ')' Statement ('else' Statement)?

  Expect(i::Token::IF, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);
  ParseStatement(CHECK_OK);
  if (peek() == i::Token::ELSE) {
    Next();
    ParseStatement(CHECK_OK);
  }
  return Statement::Default();
}


PreParser::Statement PreParser::ParseContinueStatement(bool* ok) {
  // ContinueStatement ::
  //   'continue' [no line terminator] Identifier? ';'

  Expect(i::Token::CONTINUE, CHECK_OK);
  i::Token::Value tok = peek();
  if (!scanner_->HasAnyLineTerminatorBeforeNext() &&
      tok != i::Token::SEMICOLON &&
      tok != i::Token::RBRACE &&
      tok != i::Token::EOS) {
    ParseIdentifier(CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseBreakStatement(bool* ok) {
  // BreakStatement ::
  //   'break' [no line terminator] Identifier? ';'

  Expect(i::Token::BREAK, CHECK_OK);
  i::Token::Value tok = peek();
  if (!scanner_->HasAnyLineTerminatorBeforeNext() &&
      tok != i::Token::SEMICOLON &&
      tok != i::Token::RBRACE &&
      tok != i::Token::EOS) {
    ParseIdentifier(CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseReturnStatement(bool* ok) {
  // ReturnStatement ::
  //   'return' [no line terminator] Expression? ';'

  // Consume the return token. It is necessary to do the before
  // reporting any errors on it, because of the way errors are
  // reported (underlining).
  Expect(i::Token::RETURN, CHECK_OK);

  // An ECMAScript program is considered syntactically incorrect if it
  // contains a return statement that is not within the body of a
  // function. See ECMA-262, section 12.9, page 67.
  // This is not handled during preparsing.

  i::Token::Value tok = peek();
  if (!scanner_->HasAnyLineTerminatorBeforeNext() &&
      tok != i::Token::SEMICOLON &&
      tok != i::Token::RBRACE &&
      tok != i::Token::EOS) {
    ParseExpression(true, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseWithStatement(bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement
  Expect(i::Token::WITH, CHECK_OK);
  if (!is_classic_mode()) {
    i::Scanner::Location location = scanner_->location();
    ReportMessageAt(location, "strict_mode_with", NULL);
    *ok = false;
    return Statement::Default();
  }
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);

  Scope::InsideWith iw(scope_);
  ParseStatement(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseSwitchStatement(bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'

  Expect(i::Token::SWITCH, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);

  Expect(i::Token::LBRACE, CHECK_OK);
  i::Token::Value token = peek();
  while (token != i::Token::RBRACE) {
    if (token == i::Token::CASE) {
      Expect(i::Token::CASE, CHECK_OK);
      ParseExpression(true, CHECK_OK);
    } else {
      Expect(i::Token::DEFAULT, CHECK_OK);
    }
    Expect(i::Token::COLON, CHECK_OK);
    token = peek();
    while (token != i::Token::CASE &&
           token != i::Token::DEFAULT &&
           token != i::Token::RBRACE) {
      ParseStatement(CHECK_OK);
      token = peek();
    }
  }
  Expect(i::Token::RBRACE, ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseDoWhileStatement(bool* ok) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'

  Expect(i::Token::DO, CHECK_OK);
  ParseStatement(CHECK_OK);
  Expect(i::Token::WHILE, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, ok);
  if (peek() == i::Token::SEMICOLON) Consume(i::Token::SEMICOLON);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseWhileStatement(bool* ok) {
  // WhileStatement ::
  //   'while' '(' Expression ')' Statement

  Expect(i::Token::WHILE, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);
  ParseStatement(ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseForStatement(bool* ok) {
  // ForStatement ::
  //   'for' '(' Expression? ';' Expression? ';' Expression? ')' Statement

  Expect(i::Token::FOR, CHECK_OK);
  Expect(i::Token::LPAREN, CHECK_OK);
  if (peek() != i::Token::SEMICOLON) {
    if (peek() == i::Token::VAR || peek() == i::Token::CONST ||
        peek() == i::Token::LET) {
      bool is_let = peek() == i::Token::LET;
      int decl_count;
      VariableDeclarationProperties decl_props = kHasNoInitializers;
      ParseVariableDeclarations(
          kForStatement, &decl_props, &decl_count, CHECK_OK);
      bool accept_IN = decl_count == 1 &&
          !(is_let && decl_props == kHasInitializers);
      if (peek() == i::Token::IN && accept_IN) {
        Expect(i::Token::IN, CHECK_OK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RPAREN, CHECK_OK);

        ParseStatement(CHECK_OK);
        return Statement::Default();
      }
    } else {
      ParseExpression(false, CHECK_OK);
      if (peek() == i::Token::IN) {
        Expect(i::Token::IN, CHECK_OK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RPAREN, CHECK_OK);

        ParseStatement(CHECK_OK);
        return Statement::Default();
      }
    }
  }

  // Parsed initializer at this point.
  Expect(i::Token::SEMICOLON, CHECK_OK);

  if (peek() != i::Token::SEMICOLON) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(i::Token::SEMICOLON, CHECK_OK);

  if (peek() != i::Token::RPAREN) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(i::Token::RPAREN, CHECK_OK);

  ParseStatement(ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseThrowStatement(bool* ok) {
  // ThrowStatement ::
  //   'throw' [no line terminator] Expression ';'

  Expect(i::Token::THROW, CHECK_OK);
  if (scanner_->HasAnyLineTerminatorBeforeNext()) {
    i::Scanner::Location pos = scanner_->location();
    ReportMessageAt(pos, "newline_after_throw", NULL);
    *ok = false;
    return Statement::Default();
  }
  ParseExpression(true, CHECK_OK);
  ExpectSemicolon(ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseTryStatement(bool* ok) {
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

  // In preparsing, allow any number of catch/finally blocks, including zero
  // of both.

  Expect(i::Token::TRY, CHECK_OK);

  ParseBlock(CHECK_OK);

  bool catch_or_finally_seen = false;
  if (peek() == i::Token::CATCH) {
    Consume(i::Token::CATCH);
    Expect(i::Token::LPAREN, CHECK_OK);
    Identifier id = ParseIdentifier(CHECK_OK);
    if (!is_classic_mode() && !id.IsValidStrictVariable()) {
      StrictModeIdentifierViolation(scanner_->location(),
                                    "strict_catch_variable",
                                    id,
                                    ok);
      return Statement::Default();
    }
    Expect(i::Token::RPAREN, CHECK_OK);
    { Scope::InsideWith iw(scope_);
      ParseBlock(CHECK_OK);
    }
    catch_or_finally_seen = true;
  }
  if (peek() == i::Token::FINALLY) {
    Consume(i::Token::FINALLY);
    ParseBlock(CHECK_OK);
    catch_or_finally_seen = true;
  }
  if (!catch_or_finally_seen) {
    *ok = false;
  }
  return Statement::Default();
}


PreParser::Statement PreParser::ParseDebuggerStatement(bool* ok) {
  // In ECMA-262 'debugger' is defined as a reserved keyword. In some browser
  // contexts this is used as a statement which invokes the debugger as if a
  // break point is present.
  // DebuggerStatement ::
  //   'debugger' ';'

  Expect(i::Token::DEBUGGER, CHECK_OK);
  ExpectSemicolon(ok);
  return Statement::Default();
}


#undef CHECK_OK
#define CHECK_OK  ok);                     \
  if (!*ok) return Expression::Default();  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY


// Precedence = 1
PreParser::Expression PreParser::ParseExpression(bool accept_IN, bool* ok) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  Expression result = ParseAssignmentExpression(accept_IN, CHECK_OK);
  while (peek() == i::Token::COMMA) {
    Expect(i::Token::COMMA, CHECK_OK);
    ParseAssignmentExpression(accept_IN, CHECK_OK);
    result = Expression::Default();
  }
  return result;
}


// Precedence = 2
PreParser::Expression PreParser::ParseAssignmentExpression(bool accept_IN,
                                                           bool* ok) {
  // AssignmentExpression ::
  //   ConditionalExpression
  //   YieldExpression
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression

  if (scope_->is_generator() && peek() == i::Token::YIELD) {
    return ParseYieldExpression(ok);
  }

  i::Scanner::Location before = scanner_->peek_location();
  Expression expression = ParseConditionalExpression(accept_IN, CHECK_OK);

  if (!i::Token::IsAssignmentOp(peek())) {
    // Parsed conditional expression only (no assignment).
    return expression;
  }

  if (!is_classic_mode() &&
      expression.IsIdentifier() &&
      expression.AsIdentifier().IsEvalOrArguments()) {
    i::Scanner::Location after = scanner_->location();
    ReportMessageAt(before.beg_pos, after.end_pos,
                    "strict_lhs_assignment", NULL);
    *ok = false;
    return Expression::Default();
  }

  i::Token::Value op = Next();  // Get assignment operator.
  ParseAssignmentExpression(accept_IN, CHECK_OK);

  if ((op == i::Token::ASSIGN) && expression.IsThisProperty()) {
    scope_->AddProperty();
  }

  return Expression::Default();
}


// Precedence = 3
PreParser::Expression PreParser::ParseYieldExpression(bool* ok) {
  // YieldExpression ::
  //   'yield' '*'? AssignmentExpression
  Consume(i::Token::YIELD);
  Check(i::Token::MUL);

  ParseAssignmentExpression(false, CHECK_OK);

  return Expression::Default();
}


// Precedence = 3
PreParser::Expression PreParser::ParseConditionalExpression(bool accept_IN,
                                                            bool* ok) {
  // ConditionalExpression ::
  //   LogicalOrExpression
  //   LogicalOrExpression '?' AssignmentExpression ':' AssignmentExpression

  // We start using the binary expression parser for prec >= 4 only!
  Expression expression = ParseBinaryExpression(4, accept_IN, CHECK_OK);
  if (peek() != i::Token::CONDITIONAL) return expression;
  Consume(i::Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  ParseAssignmentExpression(true, CHECK_OK);
  Expect(i::Token::COLON, CHECK_OK);
  ParseAssignmentExpression(accept_IN, CHECK_OK);
  return Expression::Default();
}


int PreParser::Precedence(i::Token::Value tok, bool accept_IN) {
  if (tok == i::Token::IN && !accept_IN)
    return 0;  // 0 precedence will terminate binary expression parsing

  return i::Token::Precedence(tok);
}


// Precedence >= 4
PreParser::Expression PreParser::ParseBinaryExpression(int prec,
                                                       bool accept_IN,
                                                       bool* ok) {
  Expression result = ParseUnaryExpression(CHECK_OK);
  for (int prec1 = Precedence(peek(), accept_IN); prec1 >= prec; prec1--) {
    // prec1 >= 4
    while (Precedence(peek(), accept_IN) == prec1) {
      Next();
      ParseBinaryExpression(prec1 + 1, accept_IN, CHECK_OK);
      result = Expression::Default();
    }
  }
  return result;
}


PreParser::Expression PreParser::ParseUnaryExpression(bool* ok) {
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

  i::Token::Value op = peek();
  if (i::Token::IsUnaryOp(op)) {
    op = Next();
    ParseUnaryExpression(ok);
    return Expression::Default();
  } else if (i::Token::IsCountOp(op)) {
    op = Next();
    i::Scanner::Location before = scanner_->peek_location();
    Expression expression = ParseUnaryExpression(CHECK_OK);
    if (!is_classic_mode() &&
        expression.IsIdentifier() &&
        expression.AsIdentifier().IsEvalOrArguments()) {
      i::Scanner::Location after = scanner_->location();
      ReportMessageAt(before.beg_pos, after.end_pos,
                      "strict_lhs_prefix", NULL);
      *ok = false;
    }
    return Expression::Default();
  } else {
    return ParsePostfixExpression(ok);
  }
}


PreParser::Expression PreParser::ParsePostfixExpression(bool* ok) {
  // PostfixExpression ::
  //   LeftHandSideExpression ('++' | '--')?

  i::Scanner::Location before = scanner_->peek_location();
  Expression expression = ParseLeftHandSideExpression(CHECK_OK);
  if (!scanner_->HasAnyLineTerminatorBeforeNext() &&
      i::Token::IsCountOp(peek())) {
    if (!is_classic_mode() &&
        expression.IsIdentifier() &&
        expression.AsIdentifier().IsEvalOrArguments()) {
      i::Scanner::Location after = scanner_->location();
      ReportMessageAt(before.beg_pos, after.end_pos,
                      "strict_lhs_postfix", NULL);
      *ok = false;
      return Expression::Default();
    }
    Next();
    return Expression::Default();
  }
  return expression;
}


PreParser::Expression PreParser::ParseLeftHandSideExpression(bool* ok) {
  // LeftHandSideExpression ::
  //   (NewExpression | MemberExpression) ...

  Expression result = Expression::Default();
  if (peek() == i::Token::NEW) {
    result = ParseNewExpression(CHECK_OK);
  } else {
    result = ParseMemberExpression(CHECK_OK);
  }

  while (true) {
    switch (peek()) {
      case i::Token::LBRACK: {
        Consume(i::Token::LBRACK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RBRACK, CHECK_OK);
        if (result.IsThis()) {
          result = Expression::ThisProperty();
        } else {
          result = Expression::Default();
        }
        break;
      }

      case i::Token::LPAREN: {
        ParseArguments(CHECK_OK);
        result = Expression::Default();
        break;
      }

      case i::Token::PERIOD: {
        Consume(i::Token::PERIOD);
        ParseIdentifierName(CHECK_OK);
        if (result.IsThis()) {
          result = Expression::ThisProperty();
        } else {
          result = Expression::Default();
        }
        break;
      }

      default:
        return result;
    }
  }
}


PreParser::Expression PreParser::ParseNewExpression(bool* ok) {
  // NewExpression ::
  //   ('new')+ MemberExpression

  // The grammar for new expressions is pretty warped. The keyword
  // 'new' can either be a part of the new expression (where it isn't
  // followed by an argument list) or a part of the member expression,
  // where it must be followed by an argument list. To accommodate
  // this, we parse the 'new' keywords greedily and keep track of how
  // many we have parsed. This information is then passed on to the
  // member expression parser, which is only allowed to match argument
  // lists as long as it has 'new' prefixes left
  unsigned new_count = 0;
  do {
    Consume(i::Token::NEW);
    new_count++;
  } while (peek() == i::Token::NEW);

  return ParseMemberWithNewPrefixesExpression(new_count, ok);
}


PreParser::Expression PreParser::ParseMemberExpression(bool* ok) {
  return ParseMemberWithNewPrefixesExpression(0, ok);
}


PreParser::Expression PreParser::ParseMemberWithNewPrefixesExpression(
    unsigned new_count, bool* ok) {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments)*

  // Parse the initial primary or function expression.
  Expression result = Expression::Default();
  if (peek() == i::Token::FUNCTION) {
    Consume(i::Token::FUNCTION);

    bool is_generator = allow_generators_ && Check(i::Token::MUL);
    Identifier identifier = Identifier::Default();
    if (peek_any_identifier()) {
      identifier = ParseIdentifier(CHECK_OK);
    }
    result = ParseFunctionLiteral(is_generator, CHECK_OK);
    if (result.IsStrictFunction() && !identifier.IsValidStrictVariable()) {
      StrictModeIdentifierViolation(scanner_->location(),
                                    "strict_function_name",
                                    identifier,
                                    ok);
      return Expression::Default();
    }
  } else {
    result = ParsePrimaryExpression(CHECK_OK);
  }

  while (true) {
    switch (peek()) {
      case i::Token::LBRACK: {
        Consume(i::Token::LBRACK);
        ParseExpression(true, CHECK_OK);
        Expect(i::Token::RBRACK, CHECK_OK);
        if (result.IsThis()) {
          result = Expression::ThisProperty();
        } else {
          result = Expression::Default();
        }
        break;
      }
      case i::Token::PERIOD: {
        Consume(i::Token::PERIOD);
        ParseIdentifierName(CHECK_OK);
        if (result.IsThis()) {
          result = Expression::ThisProperty();
        } else {
          result = Expression::Default();
        }
        break;
      }
      case i::Token::LPAREN: {
        if (new_count == 0) return result;
        // Consume one of the new prefixes (already parsed).
        ParseArguments(CHECK_OK);
        new_count--;
        result = Expression::Default();
        break;
      }
      default:
        return result;
    }
  }
}


PreParser::Expression PreParser::ParsePrimaryExpression(bool* ok) {
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

  Expression result = Expression::Default();
  switch (peek()) {
    case i::Token::THIS: {
      Next();
      result = Expression::This();
      break;
    }

    case i::Token::FUTURE_RESERVED_WORD:
    case i::Token::FUTURE_STRICT_RESERVED_WORD:
    case i::Token::YIELD:
    case i::Token::IDENTIFIER: {
      Identifier id = ParseIdentifier(CHECK_OK);
      result = Expression::FromIdentifier(id);
      break;
    }

    case i::Token::NULL_LITERAL:
    case i::Token::TRUE_LITERAL:
    case i::Token::FALSE_LITERAL:
    case i::Token::NUMBER: {
      Next();
      break;
    }
    case i::Token::STRING: {
      Next();
      result = GetStringSymbol();
      break;
    }

    case i::Token::ASSIGN_DIV:
      result = ParseRegExpLiteral(true, CHECK_OK);
      break;

    case i::Token::DIV:
      result = ParseRegExpLiteral(false, CHECK_OK);
      break;

    case i::Token::LBRACK:
      result = ParseArrayLiteral(CHECK_OK);
      break;

    case i::Token::LBRACE:
      result = ParseObjectLiteral(CHECK_OK);
      break;

    case i::Token::LPAREN:
      Consume(i::Token::LPAREN);
      parenthesized_function_ = (peek() == i::Token::FUNCTION);
      result = ParseExpression(true, CHECK_OK);
      Expect(i::Token::RPAREN, CHECK_OK);
      result = result.Parenthesize();
      break;

    case i::Token::MOD:
      result = ParseV8Intrinsic(CHECK_OK);
      break;

    default: {
      Next();
      *ok = false;
      return Expression::Default();
    }
  }

  return result;
}


PreParser::Expression PreParser::ParseArrayLiteral(bool* ok) {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'
  Expect(i::Token::LBRACK, CHECK_OK);
  while (peek() != i::Token::RBRACK) {
    if (peek() != i::Token::COMMA) {
      ParseAssignmentExpression(true, CHECK_OK);
    }
    if (peek() != i::Token::RBRACK) {
      Expect(i::Token::COMMA, CHECK_OK);
    }
  }
  Expect(i::Token::RBRACK, CHECK_OK);

  scope_->NextMaterializedLiteralIndex();
  return Expression::Default();
}

void PreParser::CheckDuplicate(DuplicateFinder* finder,
                               i::Token::Value property,
                               int type,
                               bool* ok) {
  int old_type;
  if (property == i::Token::NUMBER) {
    old_type = finder->AddNumber(scanner_->literal_ascii_string(), type);
  } else if (scanner_->is_literal_ascii()) {
    old_type = finder->AddAsciiSymbol(scanner_->literal_ascii_string(),
                                      type);
  } else {
    old_type = finder->AddUtf16Symbol(scanner_->literal_utf16_string(), type);
  }
  if (HasConflict(old_type, type)) {
    if (IsDataDataConflict(old_type, type)) {
      // Both are data properties.
      if (is_classic_mode()) return;
      ReportMessageAt(scanner_->location(),
                      "strict_duplicate_property", NULL);
    } else if (IsDataAccessorConflict(old_type, type)) {
      // Both a data and an accessor property with the same name.
      ReportMessageAt(scanner_->location(),
                      "accessor_data_property", NULL);
    } else {
      ASSERT(IsAccessorAccessorConflict(old_type, type));
      // Both accessors of the same type.
      ReportMessageAt(scanner_->location(),
                      "accessor_get_set", NULL);
    }
    *ok = false;
  }
}


PreParser::Expression PreParser::ParseObjectLiteral(bool* ok) {
  // ObjectLiteral ::
  //   '{' (
  //       ((IdentifierName | String | Number) ':' AssignmentExpression)
  //     | (('get' | 'set') (IdentifierName | String | Number) FunctionLiteral)
  //    )*[','] '}'

  Expect(i::Token::LBRACE, CHECK_OK);
  DuplicateFinder duplicate_finder(scanner_->unicode_cache());
  while (peek() != i::Token::RBRACE) {
    i::Token::Value next = peek();
    switch (next) {
      case i::Token::IDENTIFIER:
      case i::Token::FUTURE_RESERVED_WORD:
      case i::Token::FUTURE_STRICT_RESERVED_WORD: {
        bool is_getter = false;
        bool is_setter = false;
        ParseIdentifierNameOrGetOrSet(&is_getter, &is_setter, CHECK_OK);
        if ((is_getter || is_setter) && peek() != i::Token::COLON) {
            i::Token::Value name = Next();
            bool is_keyword = i::Token::IsKeyword(name);
            if (name != i::Token::IDENTIFIER &&
                name != i::Token::FUTURE_RESERVED_WORD &&
                name != i::Token::FUTURE_STRICT_RESERVED_WORD &&
                name != i::Token::NUMBER &&
                name != i::Token::STRING &&
                !is_keyword) {
              *ok = false;
              return Expression::Default();
            }
            if (!is_keyword) {
              LogSymbol();
            }
            PropertyType type = is_getter ? kGetterProperty : kSetterProperty;
            CheckDuplicate(&duplicate_finder, name, type, CHECK_OK);
            ParseFunctionLiteral(false, CHECK_OK);
            if (peek() != i::Token::RBRACE) {
              Expect(i::Token::COMMA, CHECK_OK);
            }
            continue;  // restart the while
        }
        CheckDuplicate(&duplicate_finder, next, kValueProperty, CHECK_OK);
        break;
      }
      case i::Token::STRING:
        Consume(next);
        CheckDuplicate(&duplicate_finder, next, kValueProperty, CHECK_OK);
        GetStringSymbol();
        break;
      case i::Token::NUMBER:
        Consume(next);
        CheckDuplicate(&duplicate_finder, next, kValueProperty, CHECK_OK);
        break;
      default:
        if (i::Token::IsKeyword(next)) {
          Consume(next);
          CheckDuplicate(&duplicate_finder, next, kValueProperty, CHECK_OK);
        } else {
          // Unexpected token.
          *ok = false;
          return Expression::Default();
        }
    }

    Expect(i::Token::COLON, CHECK_OK);
    ParseAssignmentExpression(true, CHECK_OK);

    // TODO(1240767): Consider allowing trailing comma.
    if (peek() != i::Token::RBRACE) Expect(i::Token::COMMA, CHECK_OK);
  }
  Expect(i::Token::RBRACE, CHECK_OK);

  scope_->NextMaterializedLiteralIndex();
  return Expression::Default();
}


PreParser::Expression PreParser::ParseRegExpLiteral(bool seen_equal,
                                                    bool* ok) {
  if (!scanner_->ScanRegExpPattern(seen_equal)) {
    Next();
    ReportMessageAt(scanner_->location(), "unterminated_regexp", NULL);
    *ok = false;
    return Expression::Default();
  }

  scope_->NextMaterializedLiteralIndex();

  if (!scanner_->ScanRegExpFlags()) {
    Next();
    ReportMessageAt(scanner_->location(), "invalid_regexp_flags", NULL);
    *ok = false;
    return Expression::Default();
  }
  Next();
  return Expression::Default();
}


PreParser::Arguments PreParser::ParseArguments(bool* ok) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  Expect(i::Token::LPAREN, ok);
  if (!*ok) return -1;
  bool done = (peek() == i::Token::RPAREN);
  int argc = 0;
  while (!done) {
    ParseAssignmentExpression(true, ok);
    if (!*ok) return -1;
    argc++;
    done = (peek() == i::Token::RPAREN);
    if (!done) {
      Expect(i::Token::COMMA, ok);
      if (!*ok) return -1;
    }
  }
  Expect(i::Token::RPAREN, ok);
  return argc;
}


PreParser::Expression PreParser::ParseFunctionLiteral(bool is_generator,
                                                      bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  ScopeType outer_scope_type = scope_->type();
  bool inside_with = scope_->IsInsideWith();
  Scope function_scope(&scope_, kFunctionScope);
  function_scope.set_is_generator(is_generator);
  //  FormalParameterList ::
  //    '(' (Identifier)*[','] ')'
  Expect(i::Token::LPAREN, CHECK_OK);
  int start_position = scanner_->location().beg_pos;
  bool done = (peek() == i::Token::RPAREN);
  DuplicateFinder duplicate_finder(scanner_->unicode_cache());
  while (!done) {
    Identifier id = ParseIdentifier(CHECK_OK);
    if (!id.IsValidStrictVariable()) {
      StrictModeIdentifierViolation(scanner_->location(),
                                    "strict_param_name",
                                    id,
                                    CHECK_OK);
    }
    int prev_value;
    if (scanner_->is_literal_ascii()) {
      prev_value =
          duplicate_finder.AddAsciiSymbol(scanner_->literal_ascii_string(), 1);
    } else {
      prev_value =
          duplicate_finder.AddUtf16Symbol(scanner_->literal_utf16_string(), 1);
    }

    if (prev_value != 0) {
      SetStrictModeViolation(scanner_->location(),
                             "strict_param_dupe",
                             CHECK_OK);
    }
    done = (peek() == i::Token::RPAREN);
    if (!done) {
      Expect(i::Token::COMMA, CHECK_OK);
    }
  }
  Expect(i::Token::RPAREN, CHECK_OK);

  // Determine if the function will be lazily compiled.
  // Currently only happens to top-level functions.
  // Optimistically assume that all top-level functions are lazily compiled.
  bool is_lazily_compiled = (outer_scope_type == kTopLevelScope &&
                             !inside_with && allow_lazy_ &&
                             !parenthesized_function_);
  parenthesized_function_ = false;

  Expect(i::Token::LBRACE, CHECK_OK);
  if (is_lazily_compiled) {
    ParseLazyFunctionLiteralBody(CHECK_OK);
  } else {
    ParseSourceElements(i::Token::RBRACE, ok);
  }
  Expect(i::Token::RBRACE, CHECK_OK);

  if (!is_classic_mode()) {
    int end_position = scanner_->location().end_pos;
    CheckOctalLiteral(start_position, end_position, CHECK_OK);
    CheckDelayedStrictModeViolation(start_position, end_position, CHECK_OK);
    return Expression::StrictFunction();
  }

  return Expression::Default();
}


void PreParser::ParseLazyFunctionLiteralBody(bool* ok) {
  int body_start = scanner_->location().beg_pos;
  log_->PauseRecording();
  ParseSourceElements(i::Token::RBRACE, ok);
  log_->ResumeRecording();
  if (!*ok) return;

  // Position right after terminal '}'.
  ASSERT_EQ(i::Token::RBRACE, scanner_->peek());
  int body_end = scanner_->peek_location().end_pos;
  log_->LogFunction(body_start, body_end,
                    scope_->materialized_literal_count(),
                    scope_->expected_properties(),
                    language_mode());
}


PreParser::Expression PreParser::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments
  Expect(i::Token::MOD, CHECK_OK);
  if (!allow_natives_syntax_) {
    *ok = false;
    return Expression::Default();
  }
  ParseIdentifier(CHECK_OK);
  ParseArguments(ok);

  return Expression::Default();
}

#undef CHECK_OK


void PreParser::ExpectSemicolon(bool* ok) {
  // Check for automatic semicolon insertion according to
  // the rules given in ECMA-262, section 7.9, page 21.
  i::Token::Value tok = peek();
  if (tok == i::Token::SEMICOLON) {
    Next();
    return;
  }
  if (scanner_->HasAnyLineTerminatorBeforeNext() ||
      tok == i::Token::RBRACE ||
      tok == i::Token::EOS) {
    return;
  }
  Expect(i::Token::SEMICOLON, ok);
}


void PreParser::LogSymbol() {
  int identifier_pos = scanner_->location().beg_pos;
  if (scanner_->is_literal_ascii()) {
    log_->LogAsciiSymbol(identifier_pos, scanner_->literal_ascii_string());
  } else {
    log_->LogUtf16Symbol(identifier_pos, scanner_->literal_utf16_string());
  }
}


PreParser::Expression PreParser::GetStringSymbol() {
  const int kUseStrictLength = 10;
  const char* kUseStrictChars = "use strict";
  LogSymbol();
  if (scanner_->is_literal_ascii() &&
      scanner_->literal_length() == kUseStrictLength &&
      !scanner_->literal_contains_escapes() &&
      !strncmp(scanner_->literal_ascii_string().start(), kUseStrictChars,
               kUseStrictLength)) {
    return Expression::UseStrictStringLiteral();
  }
  return Expression::StringLiteral();
}


PreParser::Identifier PreParser::GetIdentifierSymbol() {
  LogSymbol();
  if (scanner_->current_token() == i::Token::FUTURE_RESERVED_WORD) {
    return Identifier::FutureReserved();
  } else if (scanner_->current_token() ==
             i::Token::FUTURE_STRICT_RESERVED_WORD) {
    return Identifier::FutureStrictReserved();
  } else if (scanner_->current_token() == i::Token::YIELD) {
    return Identifier::Yield();
  }
  if (scanner_->is_literal_ascii()) {
    // Detect strict-mode poison words.
    if (scanner_->literal_length() == 4 &&
        !strncmp(scanner_->literal_ascii_string().start(), "eval", 4)) {
      return Identifier::Eval();
    }
    if (scanner_->literal_length() == 9 &&
        !strncmp(scanner_->literal_ascii_string().start(), "arguments", 9)) {
      return Identifier::Arguments();
    }
  }
  return Identifier::Default();
}


PreParser::Identifier PreParser::ParseIdentifier(bool* ok) {
  i::Token::Value next = Next();
  switch (next) {
    case i::Token::FUTURE_RESERVED_WORD: {
      i::Scanner::Location location = scanner_->location();
      ReportMessageAt(location.beg_pos, location.end_pos,
                      "reserved_word", NULL);
      *ok = false;
      return GetIdentifierSymbol();
    }
    case i::Token::YIELD:
      if (scope_->is_generator()) {
        // 'yield' in a generator is only valid as part of a YieldExpression.
        ReportMessageAt(scanner_->location(), "unexpected_token", "yield");
        *ok = false;
        return Identifier::Yield();
      }
      // FALLTHROUGH
    case i::Token::FUTURE_STRICT_RESERVED_WORD:
      if (!is_classic_mode()) {
        i::Scanner::Location location = scanner_->location();
        ReportMessageAt(location.beg_pos, location.end_pos,
                        "strict_reserved_word", NULL);
        *ok = false;
      }
      // FALLTHROUGH
    case i::Token::IDENTIFIER:
      return GetIdentifierSymbol();
    default:
      *ok = false;
      return Identifier::Default();
  }
}


void PreParser::SetStrictModeViolation(i::Scanner::Location location,
                                       const char* type,
                                       bool* ok) {
  if (!is_classic_mode()) {
    ReportMessageAt(location, type, NULL);
    *ok = false;
    return;
  }
  // Delay report in case this later turns out to be strict code
  // (i.e., for function names and parameters prior to a "use strict"
  // directive).
  // It's safe to overwrite an existing violation.
  // It's either from a function that turned out to be non-strict,
  // or it's in the current function (and we just need to report
  // one error), or it's in a unclosed nesting function that wasn't
  // strict (otherwise we would already be in strict mode).
  strict_mode_violation_location_ = location;
  strict_mode_violation_type_ = type;
}


void PreParser::CheckDelayedStrictModeViolation(int beg_pos,
                                                int end_pos,
                                                bool* ok) {
  i::Scanner::Location location = strict_mode_violation_location_;
  if (location.IsValid() &&
      location.beg_pos > beg_pos && location.end_pos < end_pos) {
    ReportMessageAt(location, strict_mode_violation_type_, NULL);
    *ok = false;
  }
}


void PreParser::StrictModeIdentifierViolation(i::Scanner::Location location,
                                              const char* eval_args_type,
                                              Identifier identifier,
                                              bool* ok) {
  const char* type = eval_args_type;
  if (identifier.IsFutureReserved()) {
    type = "reserved_word";
  } else if (identifier.IsFutureStrictReserved() || identifier.IsYield()) {
    type = "strict_reserved_word";
  }
  if (!is_classic_mode()) {
    ReportMessageAt(location, type, NULL);
    *ok = false;
    return;
  }
  strict_mode_violation_location_ = location;
  strict_mode_violation_type_ = type;
}


PreParser::Identifier PreParser::ParseIdentifierName(bool* ok) {
  i::Token::Value next = Next();
  if (i::Token::IsKeyword(next)) {
    int pos = scanner_->location().beg_pos;
    const char* keyword = i::Token::String(next);
    log_->LogAsciiSymbol(pos, i::Vector<const char>(keyword,
                                                    i::StrLength(keyword)));
    return Identifier::Default();
  }
  if (next == i::Token::IDENTIFIER ||
      next == i::Token::FUTURE_RESERVED_WORD ||
      next == i::Token::FUTURE_STRICT_RESERVED_WORD) {
    return GetIdentifierSymbol();
  }
  *ok = false;
  return Identifier::Default();
}

#undef CHECK_OK


// This function reads an identifier and determines whether or not it
// is 'get' or 'set'.
PreParser::Identifier PreParser::ParseIdentifierNameOrGetOrSet(bool* is_get,
                                                               bool* is_set,
                                                               bool* ok) {
  Identifier result = ParseIdentifierName(ok);
  if (!*ok) return Identifier::Default();
  if (scanner_->is_literal_ascii() &&
      scanner_->literal_length() == 3) {
    const char* token = scanner_->literal_ascii_string().start();
    *is_get = strncmp(token, "get", 3) == 0;
    *is_set = !*is_get && strncmp(token, "set", 3) == 0;
  }
  return result;
}

bool PreParser::peek_any_identifier() {
  i::Token::Value next = peek();
  return next == i::Token::IDENTIFIER ||
         next == i::Token::FUTURE_RESERVED_WORD ||
         next == i::Token::FUTURE_STRICT_RESERVED_WORD ||
         next == i::Token::YIELD;
}


int DuplicateFinder::AddAsciiSymbol(i::Vector<const char> key, int value) {
  return AddSymbol(i::Vector<const byte>::cast(key), true, value);
}

int DuplicateFinder::AddUtf16Symbol(i::Vector<const uint16_t> key, int value) {
  return AddSymbol(i::Vector<const byte>::cast(key), false, value);
}

int DuplicateFinder::AddSymbol(i::Vector<const byte> key,
                               bool is_ascii,
                               int value) {
  uint32_t hash = Hash(key, is_ascii);
  byte* encoding = BackupKey(key, is_ascii);
  i::HashMap::Entry* entry = map_.Lookup(encoding, hash, true);
  int old_value = static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
  entry->value =
    reinterpret_cast<void*>(static_cast<intptr_t>(value | old_value));
  return old_value;
}


int DuplicateFinder::AddNumber(i::Vector<const char> key, int value) {
  ASSERT(key.length() > 0);
  // Quick check for already being in canonical form.
  if (IsNumberCanonical(key)) {
    return AddAsciiSymbol(key, value);
  }

  int flags = i::ALLOW_HEX | i::ALLOW_OCTALS;
  double double_value = StringToDouble(unicode_constants_, key, flags, 0.0);
  int length;
  const char* string;
  if (!std::isfinite(double_value)) {
    string = "Infinity";
    length = 8;  // strlen("Infinity");
  } else {
    string = DoubleToCString(double_value,
                             i::Vector<char>(number_buffer_, kBufferSize));
    length = i::StrLength(string);
  }
  return AddSymbol(i::Vector<const byte>(reinterpret_cast<const byte*>(string),
                                         length), true, value);
}


bool DuplicateFinder::IsNumberCanonical(i::Vector<const char> number) {
  // Test for a safe approximation of number literals that are already
  // in canonical form: max 15 digits, no leading zeroes, except an
  // integer part that is a single zero, and no trailing zeros below
  // the decimal point.
  int pos = 0;
  int length = number.length();
  if (number.length() > 15) return false;
  if (number[pos] == '0') {
    pos++;
  } else {
    while (pos < length &&
           static_cast<unsigned>(number[pos] - '0') <= ('9' - '0')) pos++;
  }
  if (length == pos) return true;
  if (number[pos] != '.') return false;
  pos++;
  bool invalid_last_digit = true;
  while (pos < length) {
    byte digit = number[pos] - '0';
    if (digit > '9' - '0') return false;
    invalid_last_digit = (digit == 0);
    pos++;
  }
  return !invalid_last_digit;
}


uint32_t DuplicateFinder::Hash(i::Vector<const byte> key, bool is_ascii) {
  // Primitive hash function, almost identical to the one used
  // for strings (except that it's seeded by the length and ASCII-ness).
  int length = key.length();
  uint32_t hash = (length << 1) | (is_ascii ? 1 : 0) ;
  for (int i = 0; i < length; i++) {
    uint32_t c = key[i];
    hash = (hash + c) * 1025;
    hash ^= (hash >> 6);
  }
  return hash;
}


bool DuplicateFinder::Match(void* first, void* second) {
  // Decode lengths.
  // Length + ASCII-bit is encoded as base 128, most significant heptet first,
  // with a 8th bit being non-zero while there are more heptets.
  // The value encodes the number of bytes following, and whether the original
  // was ASCII.
  byte* s1 = reinterpret_cast<byte*>(first);
  byte* s2 = reinterpret_cast<byte*>(second);
  uint32_t length_ascii_field = 0;
  byte c1;
  do {
    c1 = *s1;
    if (c1 != *s2) return false;
    length_ascii_field = (length_ascii_field << 7) | (c1 & 0x7f);
    s1++;
    s2++;
  } while ((c1 & 0x80) != 0);
  int length = static_cast<int>(length_ascii_field >> 1);
  return memcmp(s1, s2, length) == 0;
}


byte* DuplicateFinder::BackupKey(i::Vector<const byte> bytes,
                                 bool is_ascii) {
  uint32_t ascii_length = (bytes.length() << 1) | (is_ascii ? 1 : 0);
  backing_store_.StartSequence();
  // Emit ascii_length as base-128 encoded number, with the 7th bit set
  // on the byte of every heptet except the last, least significant, one.
  if (ascii_length >= (1 << 7)) {
    if (ascii_length >= (1 << 14)) {
      if (ascii_length >= (1 << 21)) {
        if (ascii_length >= (1 << 28)) {
          backing_store_.Add(static_cast<byte>((ascii_length >> 28) | 0x80));
        }
        backing_store_.Add(static_cast<byte>((ascii_length >> 21) | 0x80u));
      }
      backing_store_.Add(static_cast<byte>((ascii_length >> 14) | 0x80u));
    }
    backing_store_.Add(static_cast<byte>((ascii_length >> 7) | 0x80u));
  }
  backing_store_.Add(static_cast<byte>(ascii_length & 0x7f));

  backing_store_.AddBlock(bytes);
  return backing_store_.EndSequence().start();
}
} }  // v8::preparser
