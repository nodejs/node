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

#include "../include/v8stdint.h"
#include "unicode.h"
#include "globals.h"
#include "checks.h"
#include "allocation.h"
#include "utils.h"
#include "list.h"

#include "scanner-base.h"
#include "preparse-data-format.h"
#include "preparse-data.h"
#include "preparser.h"

#include "conversions-inl.h"

namespace v8 {
namespace preparser {

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
  i::JavaScriptScanner::Location source_location = scanner_->location();

  // Four of the tokens are treated specially
  switch (token) {
  case i::Token::EOS:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_eos", NULL);
  case i::Token::NUMBER:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_token_number", NULL);
  case i::Token::STRING:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_token_string", NULL);
  case i::Token::IDENTIFIER:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_token_identifier", NULL);
  case i::Token::FUTURE_RESERVED_WORD:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_reserved", NULL);
  case i::Token::FUTURE_STRICT_RESERVED_WORD:
    return ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                           "unexpected_strict_reserved", NULL);
  default:
    const char* name = i::Token::String(token);
    ReportMessageAt(source_location.beg_pos, source_location.end_pos,
                    "unexpected_token", name);
  }
}


// Checks whether octal literal last seen is between beg_pos and end_pos.
// If so, reports an error.
void PreParser::CheckOctalLiteral(int beg_pos, int end_pos, bool* ok) {
  i::Scanner::Location octal = scanner_->octal_position();
  if (beg_pos <= octal.beg_pos && octal.end_pos <= end_pos) {
    ReportMessageAt(octal.beg_pos, octal.end_pos, "strict_octal_literal", NULL);
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
  switch (peek()) {
    case i::Token::LET:
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
        set_strict_mode();
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

    case i::Token::FUNCTION:
      return ParseFunctionDeclaration(ok);

    case i::Token::DEBUGGER:
      return ParseDebuggerStatement(ok);

    default:
      return ParseExpressionOrLabelledStatement(ok);
  }
}


PreParser::Statement PreParser::ParseFunctionDeclaration(bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameterListopt ')' '{' FunctionBody '}'
  Expect(i::Token::FUNCTION, CHECK_OK);

  Identifier identifier = ParseIdentifier(CHECK_OK);
  i::Scanner::Location location = scanner_->location();

  Expression function_value = ParseFunctionLiteral(CHECK_OK);

  if (function_value.IsStrictFunction() &&
      !identifier.IsValidStrictVariable()) {
    // Strict mode violation, using either reserved word or eval/arguments
    // as name of strict function.
    const char* type = "strict_function_name";
    if (identifier.IsFutureStrictReserved()) {
      type = "strict_reserved_word";
    }
    ReportMessageAt(location.beg_pos, location.end_pos, type, NULL);
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
    i::Scanner::Location start_location = scanner_->peek_location();
    Statement statement = ParseSourceElement(CHECK_OK);
    i::Scanner::Location end_location = scanner_->location();
    if (strict_mode() && statement.IsFunctionDeclaration()) {
      ReportMessageAt(start_location.beg_pos, end_location.end_pos,
                      "strict_function", NULL);
      *ok = false;
      return Statement::Default();
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
    int* num_decl,
    bool* ok) {
  // VariableDeclarations ::
  //   ('var' | 'const') (Identifier ('=' AssignmentExpression)?)+[',']

  if (peek() == i::Token::VAR) {
    Consume(i::Token::VAR);
  } else if (peek() == i::Token::CONST) {
    if (strict_mode()) {
      i::Scanner::Location location = scanner_->peek_location();
      ReportMessageAt(location.beg_pos, location.end_pos,
                      "strict_const", NULL);
      *ok = false;
      return Statement::Default();
    }
    Consume(i::Token::CONST);
  } else if (peek() == i::Token::LET) {
    if (var_context != kSourceElement &&
        var_context != kForStatement) {
      i::Scanner::Location location = scanner_->peek_location();
      ReportMessageAt(location.beg_pos, location.end_pos,
                      "unprotected_let", NULL);
      *ok = false;
      return Statement::Default();
    }
    Consume(i::Token::LET);
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
    if (strict_mode() && !identifier.IsValidStrictVariable()) {
      StrictModeIdentifierViolation(scanner_->location(),
                                    "strict_var_name",
                                    identifier,
                                    ok);
      return Statement::Default();
    }
    nvars++;
    if (peek() == i::Token::ASSIGN) {
      Expect(i::Token::ASSIGN, CHECK_OK);
      ParseAssignmentExpression(var_context != kForStatement, CHECK_OK);
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
    if (peek() == i::Token::COLON &&
        (!strict_mode() || !expr.AsIdentifier().IsFutureReserved())) {
      Consume(i::Token::COLON);
      i::Scanner::Location start_location = scanner_->peek_location();
      Statement statement = ParseStatement(CHECK_OK);
      if (strict_mode() && statement.IsFunctionDeclaration()) {
        i::Scanner::Location end_location = scanner_->location();
        ReportMessageAt(start_location.beg_pos, end_location.end_pos,
                        "strict_function", NULL);
        *ok = false;
      }
      return Statement::Default();
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
  if (strict_mode()) {
    i::Scanner::Location location = scanner_->location();
    ReportMessageAt(location.beg_pos, location.end_pos,
                    "strict_mode_with", NULL);
    *ok = false;
    return Statement::Default();
  }
  Expect(i::Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(i::Token::RPAREN, CHECK_OK);

  scope_->EnterWith();
  ParseStatement(CHECK_OK);
  scope_->LeaveWith();
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
      Expect(i::Token::COLON, CHECK_OK);
    } else if (token == i::Token::DEFAULT) {
      Expect(i::Token::DEFAULT, CHECK_OK);
      Expect(i::Token::COLON, CHECK_OK);
    } else {
      i::Scanner::Location start_location = scanner_->peek_location();
      Statement statement = ParseStatement(CHECK_OK);
      if (strict_mode() && statement.IsFunctionDeclaration()) {
        i::Scanner::Location end_location = scanner_->location();
        ReportMessageAt(start_location.beg_pos, end_location.end_pos,
                        "strict_function", NULL);
        *ok = false;
        return Statement::Default();
      }
    }
    token = peek();
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
      int decl_count;
      ParseVariableDeclarations(kForStatement, &decl_count, CHECK_OK);
      if (peek() == i::Token::IN && decl_count == 1) {
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
    i::JavaScriptScanner::Location pos = scanner_->location();
    ReportMessageAt(pos.beg_pos, pos.end_pos,
                    "newline_after_throw", NULL);
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
    if (strict_mode() && !id.IsValidStrictVariable()) {
      StrictModeIdentifierViolation(scanner_->location(),
                                    "strict_catch_variable",
                                    id,
                                    ok);
      return Statement::Default();
    }
    Expect(i::Token::RPAREN, CHECK_OK);
    scope_->EnterWith();
    ParseBlock(ok);
    scope_->LeaveWith();
    if (!*ok) Statement::Default();
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
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression

  i::Scanner::Location before = scanner_->peek_location();
  Expression expression = ParseConditionalExpression(accept_IN, CHECK_OK);

  if (!i::Token::IsAssignmentOp(peek())) {
    // Parsed conditional expression only (no assignment).
    return expression;
  }

  if (strict_mode() && expression.IsIdentifier() &&
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
    if (strict_mode() && expression.IsIdentifier() &&
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
    if (strict_mode() && expression.IsIdentifier() &&
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
    Identifier identifier = Identifier::Default();
    if (peek_any_identifier()) {
      identifier = ParseIdentifier(CHECK_OK);
    }
    result = ParseFunctionLiteral(CHECK_OK);
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

    case i::Token::FUTURE_RESERVED_WORD: {
      Next();
      i::Scanner::Location location = scanner_->location();
      ReportMessageAt(location.beg_pos, location.end_pos,
                      "reserved_word", NULL);
      *ok = false;
      return Expression::Default();
    }

    case i::Token::FUTURE_STRICT_RESERVED_WORD:
      if (strict_mode()) {
        Next();
        i::Scanner::Location location = scanner_->location();
        ReportMessageAt(location.beg_pos, location.end_pos,
                        "strict_reserved_word", NULL);
        *ok = false;
        return Expression::Default();
      }
      // FALLTHROUGH
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


PreParser::Expression PreParser::ParseObjectLiteral(bool* ok) {
  // ObjectLiteral ::
  //   '{' (
  //       ((IdentifierName | String | Number) ':' AssignmentExpression)
  //     | (('get' | 'set') (IdentifierName | String | Number) FunctionLiteral)
  //    )*[','] '}'

  Expect(i::Token::LBRACE, CHECK_OK);
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
            ParseFunctionLiteral(CHECK_OK);
            if (peek() != i::Token::RBRACE) {
              Expect(i::Token::COMMA, CHECK_OK);
            }
            continue;  // restart the while
        }
        break;
      }
      case i::Token::STRING:
        Consume(next);
        GetStringSymbol();
        break;
      case i::Token::NUMBER:
        Consume(next);
        break;
      default:
        if (i::Token::IsKeyword(next)) {
          Consume(next);
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
    i::JavaScriptScanner::Location location = scanner_->location();
    ReportMessageAt(location.beg_pos, location.end_pos,
                    "unterminated_regexp", NULL);
    *ok = false;
    return Expression::Default();
  }

  scope_->NextMaterializedLiteralIndex();

  if (!scanner_->ScanRegExpFlags()) {
    Next();
    i::JavaScriptScanner::Location location = scanner_->location();
    ReportMessageAt(location.beg_pos, location.end_pos,
                    "invalid_regexp_flags", NULL);
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


PreParser::Expression PreParser::ParseFunctionLiteral(bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  ScopeType outer_scope_type = scope_->type();
  bool inside_with = scope_->IsInsideWith();
  Scope function_scope(&scope_, kFunctionScope);
  //  FormalParameterList ::
  //    '(' (Identifier)*[','] ')'
  Expect(i::Token::LPAREN, CHECK_OK);
  int start_position = scanner_->location().beg_pos;
  bool done = (peek() == i::Token::RPAREN);
  while (!done) {
    Identifier id = ParseIdentifier(CHECK_OK);
    if (!id.IsValidStrictVariable()) {
      StrictModeIdentifierViolation(scanner_->location(),
                                    "strict_param_name",
                                    id,
                                    CHECK_OK);
    }
    done = (peek() == i::Token::RPAREN);
    if (!done) {
      Expect(i::Token::COMMA, CHECK_OK);
    }
  }
  Expect(i::Token::RPAREN, CHECK_OK);

  Expect(i::Token::LBRACE, CHECK_OK);
  int function_block_pos = scanner_->location().beg_pos;

  // Determine if the function will be lazily compiled.
  // Currently only happens to top-level functions.
  // Optimistically assume that all top-level functions are lazily compiled.
  bool is_lazily_compiled = (outer_scope_type == kTopLevelScope &&
                             !inside_with && allow_lazy_ &&
                             !parenthesized_function_);
  parenthesized_function_ = false;

  if (is_lazily_compiled) {
    log_->PauseRecording();
    ParseSourceElements(i::Token::RBRACE, ok);
    log_->ResumeRecording();
    if (!*ok) Expression::Default();

    Expect(i::Token::RBRACE, CHECK_OK);

    // Position right after terminal '}'.
    int end_pos = scanner_->location().end_pos;
    log_->LogFunction(function_block_pos, end_pos,
                      function_scope.materialized_literal_count(),
                      function_scope.expected_properties(),
                      strict_mode() ? 1 : 0);
  } else {
    ParseSourceElements(i::Token::RBRACE, CHECK_OK);
    Expect(i::Token::RBRACE, CHECK_OK);
  }

  if (strict_mode()) {
    int end_position = scanner_->location().end_pos;
    CheckOctalLiteral(start_position, end_position, CHECK_OK);
    CheckDelayedStrictModeViolation(start_position, end_position, CHECK_OK);
    return Expression::StrictFunction();
  }

  return Expression::Default();
}


PreParser::Expression PreParser::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments

  Expect(i::Token::MOD, CHECK_OK);
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
    log_->LogUC16Symbol(identifier_pos, scanner_->literal_uc16_string());
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
    }
      // FALLTHROUGH
    case i::Token::FUTURE_STRICT_RESERVED_WORD:
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
  if (strict_mode()) {
    ReportMessageAt(location.beg_pos, location.end_pos, type, NULL);
    *ok = false;
    return;
  }
  // Delay report in case this later turns out to be strict code
  // (i.e., for function names and parameters prior to a "use strict"
  // directive).
  strict_mode_violation_location_ = location;
  strict_mode_violation_type_ = type;
}


void PreParser::CheckDelayedStrictModeViolation(int beg_pos,
                                                int end_pos,
                                                bool* ok) {
  i::Scanner::Location location = strict_mode_violation_location_;
  if (location.IsValid() &&
      location.beg_pos > beg_pos && location.end_pos < end_pos) {
    ReportMessageAt(location.beg_pos, location.end_pos,
                    strict_mode_violation_type_, NULL);
    *ok = false;
  }
  strict_mode_violation_location_ = i::Scanner::Location::invalid();
}


void PreParser::StrictModeIdentifierViolation(i::Scanner::Location location,
                                              const char* eval_args_type,
                                              Identifier identifier,
                                              bool* ok) {
  const char* type = eval_args_type;
  if (identifier.IsFutureReserved()) {
    type = "reserved_word";
  } else if (identifier.IsFutureStrictReserved()) {
    type = "strict_reserved_word";
  }
  if (strict_mode()) {
    ReportMessageAt(location.beg_pos, location.end_pos, type, NULL);
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
         next == i::Token::FUTURE_STRICT_RESERVED_WORD;
}
} }  // v8::preparser
