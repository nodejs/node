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

#if V8_LIBC_MSVCRT && (_MSC_VER < 1800)
namespace std {

// Usually defined in math.h, but not in MSVC until VS2013+.
// Abstracted to work
int isfinite(double value);

}  // namespace std
#endif

namespace v8 {
namespace internal {

void PreParserTraits::ReportMessageAt(Scanner::Location location,
                                      const char* message,
                                      Vector<const char*> args) {
  ReportMessageAt(location.beg_pos,
                  location.end_pos,
                  message,
                  args.length() > 0 ? args[0] : NULL);
}


void PreParserTraits::ReportMessageAt(Scanner::Location location,
                                      const char* type,
                                      const char* name_opt) {
  pre_parser_->log_
      ->LogMessage(location.beg_pos, location.end_pos, type, name_opt);
}


void PreParserTraits::ReportMessageAt(int start_pos,
                                      int end_pos,
                                      const char* type,
                                      const char* name_opt) {
  pre_parser_->log_->LogMessage(start_pos, end_pos, type, name_opt);
}


PreParserIdentifier PreParserTraits::GetSymbol(Scanner* scanner) {
  pre_parser_->LogSymbol();
  if (scanner->current_token() == Token::FUTURE_RESERVED_WORD) {
    return PreParserIdentifier::FutureReserved();
  } else if (scanner->current_token() ==
             Token::FUTURE_STRICT_RESERVED_WORD) {
    return PreParserIdentifier::FutureStrictReserved();
  } else if (scanner->current_token() == Token::YIELD) {
    return PreParserIdentifier::Yield();
  }
  if (scanner->is_literal_ascii()) {
    // Detect strict-mode poison words.
    if (scanner->literal_length() == 4 &&
        !strncmp(scanner->literal_ascii_string().start(), "eval", 4)) {
      return PreParserIdentifier::Eval();
    }
    if (scanner->literal_length() == 9 &&
        !strncmp(scanner->literal_ascii_string().start(), "arguments", 9)) {
      return PreParserIdentifier::Arguments();
    }
  }
  return PreParserIdentifier::Default();
}


PreParserExpression PreParserTraits::ExpressionFromString(
    int pos, Scanner* scanner, PreParserFactory* factory) {
  const int kUseStrictLength = 10;
  const char* kUseStrictChars = "use strict";
  pre_parser_->LogSymbol();
  if (scanner->is_literal_ascii() &&
      scanner->literal_length() == kUseStrictLength &&
      !scanner->literal_contains_escapes() &&
      !strncmp(scanner->literal_ascii_string().start(), kUseStrictChars,
               kUseStrictLength)) {
    return PreParserExpression::UseStrictStringLiteral();
  }
  return PreParserExpression::StringLiteral();
}


PreParserExpression PreParserTraits::ParseArrayLiteral(bool* ok) {
  return pre_parser_->ParseArrayLiteral(ok);
}


PreParserExpression PreParserTraits::ParseObjectLiteral(bool* ok) {
  return pre_parser_->ParseObjectLiteral(ok);
}


PreParserExpression PreParserTraits::ParseExpression(bool accept_IN, bool* ok) {
  return pre_parser_->ParseExpression(accept_IN, ok);
}


PreParserExpression PreParserTraits::ParseV8Intrinsic(bool* ok) {
  return pre_parser_->ParseV8Intrinsic(ok);
}


PreParser::PreParseResult PreParser::PreParseLazyFunction(
    LanguageMode mode, bool is_generator, ParserRecorder* log) {
  log_ = log;
  // Lazy functions always have trivial outer scopes (no with/catch scopes).
  PreParserScope top_scope(scope_, GLOBAL_SCOPE);
  FunctionState top_state(&function_state_, &scope_, &top_scope);
  scope_->SetLanguageMode(mode);
  PreParserScope function_scope(scope_, FUNCTION_SCOPE);
  FunctionState function_state(&function_state_, &scope_, &function_scope);
  function_state.set_is_generator(is_generator);
  ASSERT_EQ(Token::LBRACE, scanner()->current_token());
  bool ok = true;
  int start_position = peek_position();
  ParseLazyFunctionLiteralBody(&ok);
  if (stack_overflow()) return kPreParseStackOverflow;
  if (!ok) {
    ReportUnexpectedToken(scanner()->current_token());
  } else {
    ASSERT_EQ(Token::RBRACE, scanner()->peek());
    if (!scope_->is_classic_mode()) {
      int end_pos = scanner()->location().end_pos;
      CheckOctalLiteral(start_position, end_pos, &ok);
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
    case Token::FUNCTION:
      return ParseFunctionDeclaration(ok);
    case Token::LET:
    case Token::CONST:
      return ParseVariableStatement(kSourceElement, ok);
    default:
      return ParseStatement(ok);
  }
}


PreParser::SourceElements PreParser::ParseSourceElements(int end_token,
                                                         bool* ok) {
  // SourceElements ::
  //   (Statement)* <end_token>

  bool directive_prologue = true;
  while (peek() != end_token) {
    if (directive_prologue && peek() != Token::STRING) {
      directive_prologue = false;
    }
    Statement statement = ParseSourceElement(CHECK_OK);
    if (directive_prologue) {
      if (statement.IsUseStrictLiteral()) {
        scope_->SetLanguageMode(allow_harmony_scoping() ?
                                EXTENDED_MODE : STRICT_MODE);
      } else if (!statement.IsStringLiteral()) {
        directive_prologue = false;
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
    case Token::LBRACE:
      return ParseBlock(ok);

    case Token::CONST:
    case Token::LET:
    case Token::VAR:
      return ParseVariableStatement(kStatement, ok);

    case Token::SEMICOLON:
      Next();
      return Statement::Default();

    case Token::IF:
      return ParseIfStatement(ok);

    case Token::DO:
      return ParseDoWhileStatement(ok);

    case Token::WHILE:
      return ParseWhileStatement(ok);

    case Token::FOR:
      return ParseForStatement(ok);

    case Token::CONTINUE:
      return ParseContinueStatement(ok);

    case Token::BREAK:
      return ParseBreakStatement(ok);

    case Token::RETURN:
      return ParseReturnStatement(ok);

    case Token::WITH:
      return ParseWithStatement(ok);

    case Token::SWITCH:
      return ParseSwitchStatement(ok);

    case Token::THROW:
      return ParseThrowStatement(ok);

    case Token::TRY:
      return ParseTryStatement(ok);

    case Token::FUNCTION: {
      Scanner::Location start_location = scanner()->peek_location();
      Statement statement = ParseFunctionDeclaration(CHECK_OK);
      Scanner::Location end_location = scanner()->location();
      if (!scope_->is_classic_mode()) {
        PreParserTraits::ReportMessageAt(start_location.beg_pos,
                                         end_location.end_pos,
                                         "strict_function",
                                         NULL);
        *ok = false;
        return Statement::Default();
      } else {
        return statement;
      }
    }

    case Token::DEBUGGER:
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
  Expect(Token::FUNCTION, CHECK_OK);

  bool is_generator = allow_generators() && Check(Token::MUL);
  bool is_strict_reserved = false;
  Identifier name = ParseIdentifierOrStrictReservedWord(
      &is_strict_reserved, CHECK_OK);
  ParseFunctionLiteral(name,
                       scanner()->location(),
                       is_strict_reserved,
                       is_generator,
                       CHECK_OK);
  return Statement::FunctionDeclaration();
}


PreParser::Statement PreParser::ParseBlock(bool* ok) {
  // Block ::
  //   '{' Statement* '}'

  // Note that a Block does not introduce a new execution scope!
  // (ECMA-262, 3rd, 12.2)
  //
  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    if (scope_->is_extended_mode()) {
      ParseSourceElement(CHECK_OK);
    } else {
      ParseStatement(CHECK_OK);
    }
  }
  Expect(Token::RBRACE, ok);
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
  if (peek() == Token::VAR) {
    Consume(Token::VAR);
  } else if (peek() == Token::CONST) {
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
    Consume(Token::CONST);
    switch (scope_->language_mode()) {
      case CLASSIC_MODE:
        break;
      case STRICT_MODE: {
        Scanner::Location location = scanner()->peek_location();
        ReportMessageAt(location, "strict_const");
        *ok = false;
        return Statement::Default();
      }
      case EXTENDED_MODE:
        if (var_context != kSourceElement &&
            var_context != kForStatement) {
          ReportMessageAt(scanner()->peek_location(), "unprotected_const");
          *ok = false;
          return Statement::Default();
        }
        require_initializer = true;
        break;
    }
  } else if (peek() == Token::LET) {
    // ES6 Draft Rev4 section 12.2.1:
    //
    // LetDeclaration : let LetBindingList ;
    //
    // * It is a Syntax Error if the code that matches this production is not
    //   contained in extended code.
    if (!scope_->is_extended_mode()) {
      ReportMessageAt(scanner()->peek_location(), "illegal_let");
      *ok = false;
      return Statement::Default();
    }
    Consume(Token::LET);
    if (var_context != kSourceElement &&
        var_context != kForStatement) {
      ReportMessageAt(scanner()->peek_location(), "unprotected_let");
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
    if (nvars > 0) Consume(Token::COMMA);
    ParseIdentifier(kDontAllowEvalOrArguments, CHECK_OK);
    nvars++;
    if (peek() == Token::ASSIGN || require_initializer) {
      Expect(Token::ASSIGN, CHECK_OK);
      ParseAssignmentExpression(var_context != kForStatement, CHECK_OK);
      if (decl_props != NULL) *decl_props = kHasInitializers;
    }
  } while (peek() == Token::COMMA);

  if (num_decl != NULL) *num_decl = nvars;
  return Statement::Default();
}


PreParser::Statement PreParser::ParseExpressionOrLabelledStatement(bool* ok) {
  // ExpressionStatement | LabelledStatement ::
  //   Expression ';'
  //   Identifier ':' Statement

  bool starts_with_identifier = peek_any_identifier();
  Expression expr = ParseExpression(true, CHECK_OK);
  // Even if the expression starts with an identifier, it is not necessarily an
  // identifier. For example, "foo + bar" starts with an identifier but is not
  // an identifier.
  if (starts_with_identifier && expr.IsIdentifier() && peek() == Token::COLON) {
    // Expression is a single identifier, and not, e.g., a parenthesized
    // identifier.
    ASSERT(!expr.AsIdentifier().IsFutureReserved());
    ASSERT(scope_->is_classic_mode() ||
           (!expr.AsIdentifier().IsFutureStrictReserved() &&
            !expr.AsIdentifier().IsYield()));
    Consume(Token::COLON);
    return ParseStatement(ok);
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

  Expect(Token::IF, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  ParseStatement(CHECK_OK);
  if (peek() == Token::ELSE) {
    Next();
    ParseStatement(CHECK_OK);
  }
  return Statement::Default();
}


PreParser::Statement PreParser::ParseContinueStatement(bool* ok) {
  // ContinueStatement ::
  //   'continue' [no line terminator] Identifier? ';'

  Expect(Token::CONTINUE, CHECK_OK);
  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      tok != Token::SEMICOLON &&
      tok != Token::RBRACE &&
      tok != Token::EOS) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseBreakStatement(bool* ok) {
  // BreakStatement ::
  //   'break' [no line terminator] Identifier? ';'

  Expect(Token::BREAK, CHECK_OK);
  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      tok != Token::SEMICOLON &&
      tok != Token::RBRACE &&
      tok != Token::EOS) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
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
  Expect(Token::RETURN, CHECK_OK);

  // An ECMAScript program is considered syntactically incorrect if it
  // contains a return statement that is not within the body of a
  // function. See ECMA-262, section 12.9, page 67.
  // This is not handled during preparsing.

  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      tok != Token::SEMICOLON &&
      tok != Token::RBRACE &&
      tok != Token::EOS) {
    ParseExpression(true, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseWithStatement(bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement
  Expect(Token::WITH, CHECK_OK);
  if (!scope_->is_classic_mode()) {
    ReportMessageAt(scanner()->location(), "strict_mode_with");
    *ok = false;
    return Statement::Default();
  }
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  PreParserScope with_scope(scope_, WITH_SCOPE);
  BlockState block_state(&scope_, &with_scope);
  ParseStatement(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseSwitchStatement(bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'

  Expect(Token::SWITCH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  Expect(Token::LBRACE, CHECK_OK);
  Token::Value token = peek();
  while (token != Token::RBRACE) {
    if (token == Token::CASE) {
      Expect(Token::CASE, CHECK_OK);
      ParseExpression(true, CHECK_OK);
    } else {
      Expect(Token::DEFAULT, CHECK_OK);
    }
    Expect(Token::COLON, CHECK_OK);
    token = peek();
    while (token != Token::CASE &&
           token != Token::DEFAULT &&
           token != Token::RBRACE) {
      ParseStatement(CHECK_OK);
      token = peek();
    }
  }
  Expect(Token::RBRACE, ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseDoWhileStatement(bool* ok) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'

  Expect(Token::DO, CHECK_OK);
  ParseStatement(CHECK_OK);
  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, ok);
  if (peek() == Token::SEMICOLON) Consume(Token::SEMICOLON);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseWhileStatement(bool* ok) {
  // WhileStatement ::
  //   'while' '(' Expression ')' Statement

  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  ParseStatement(ok);
  return Statement::Default();
}


bool PreParser::CheckInOrOf(bool accept_OF) {
  if (Check(Token::IN) ||
      (allow_for_of() && accept_OF &&
       CheckContextualKeyword(CStrVector("of")))) {
    return true;
  }
  return false;
}


PreParser::Statement PreParser::ParseForStatement(bool* ok) {
  // ForStatement ::
  //   'for' '(' Expression? ';' Expression? ';' Expression? ')' Statement

  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  if (peek() != Token::SEMICOLON) {
    if (peek() == Token::VAR || peek() == Token::CONST ||
        peek() == Token::LET) {
      bool is_let = peek() == Token::LET;
      int decl_count;
      VariableDeclarationProperties decl_props = kHasNoInitializers;
      ParseVariableDeclarations(
          kForStatement, &decl_props, &decl_count, CHECK_OK);
      bool has_initializers = decl_props == kHasInitializers;
      bool accept_IN = decl_count == 1 && !(is_let && has_initializers);
      bool accept_OF = !has_initializers;
      if (accept_IN && CheckInOrOf(accept_OF)) {
        ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);

        ParseStatement(CHECK_OK);
        return Statement::Default();
      }
    } else {
      Expression lhs = ParseExpression(false, CHECK_OK);
      if (CheckInOrOf(lhs.IsIdentifier())) {
        ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);

        ParseStatement(CHECK_OK);
        return Statement::Default();
      }
    }
  }

  // Parsed initializer at this point.
  Expect(Token::SEMICOLON, CHECK_OK);

  if (peek() != Token::SEMICOLON) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(Token::SEMICOLON, CHECK_OK);

  if (peek() != Token::RPAREN) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(Token::RPAREN, CHECK_OK);

  ParseStatement(ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseThrowStatement(bool* ok) {
  // ThrowStatement ::
  //   'throw' [no line terminator] Expression ';'

  Expect(Token::THROW, CHECK_OK);
  if (scanner()->HasAnyLineTerminatorBeforeNext()) {
    ReportMessageAt(scanner()->location(), "newline_after_throw");
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

  Expect(Token::TRY, CHECK_OK);

  ParseBlock(CHECK_OK);

  Token::Value tok = peek();
  if (tok != Token::CATCH && tok != Token::FINALLY) {
    ReportMessageAt(scanner()->location(), "no_catch_or_finally");
    *ok = false;
    return Statement::Default();
  }
  if (tok == Token::CATCH) {
    Consume(Token::CATCH);
    Expect(Token::LPAREN, CHECK_OK);
    ParseIdentifier(kDontAllowEvalOrArguments, CHECK_OK);
    Expect(Token::RPAREN, CHECK_OK);
    {
      PreParserScope with_scope(scope_, WITH_SCOPE);
      BlockState block_state(&scope_, &with_scope);
      ParseBlock(CHECK_OK);
    }
    tok = peek();
  }
  if (tok == Token::FINALLY) {
    Consume(Token::FINALLY);
    ParseBlock(CHECK_OK);
  }
  return Statement::Default();
}


PreParser::Statement PreParser::ParseDebuggerStatement(bool* ok) {
  // In ECMA-262 'debugger' is defined as a reserved keyword. In some browser
  // contexts this is used as a statement which invokes the debugger as if a
  // break point is present.
  // DebuggerStatement ::
  //   'debugger' ';'

  Expect(Token::DEBUGGER, CHECK_OK);
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
  while (peek() == Token::COMMA) {
    Expect(Token::COMMA, CHECK_OK);
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

  if (function_state_->is_generator() && peek() == Token::YIELD) {
    return ParseYieldExpression(ok);
  }

  Scanner::Location before = scanner()->peek_location();
  Expression expression = ParseConditionalExpression(accept_IN, CHECK_OK);

  if (!Token::IsAssignmentOp(peek())) {
    // Parsed conditional expression only (no assignment).
    return expression;
  }

  if (!scope_->is_classic_mode() &&
      expression.IsIdentifier() &&
      expression.AsIdentifier().IsEvalOrArguments()) {
    Scanner::Location after = scanner()->location();
    PreParserTraits::ReportMessageAt(before.beg_pos, after.end_pos,
                                     "strict_eval_arguments", NULL);
    *ok = false;
    return Expression::Default();
  }

  Token::Value op = Next();  // Get assignment operator.
  ParseAssignmentExpression(accept_IN, CHECK_OK);

  if ((op == Token::ASSIGN) && expression.IsThisProperty()) {
    function_state_->AddProperty();
  }

  return Expression::Default();
}


// Precedence = 3
PreParser::Expression PreParser::ParseYieldExpression(bool* ok) {
  // YieldExpression ::
  //   'yield' '*'? AssignmentExpression
  Consume(Token::YIELD);
  Check(Token::MUL);

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
  if (peek() != Token::CONDITIONAL) return expression;
  Consume(Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  ParseAssignmentExpression(true, CHECK_OK);
  Expect(Token::COLON, CHECK_OK);
  ParseAssignmentExpression(accept_IN, CHECK_OK);
  return Expression::Default();
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

  Token::Value op = peek();
  if (Token::IsUnaryOp(op)) {
    op = Next();
    ParseUnaryExpression(ok);
    return Expression::Default();
  } else if (Token::IsCountOp(op)) {
    op = Next();
    Scanner::Location before = scanner()->peek_location();
    Expression expression = ParseUnaryExpression(CHECK_OK);
    if (!scope_->is_classic_mode() &&
        expression.IsIdentifier() &&
        expression.AsIdentifier().IsEvalOrArguments()) {
      Scanner::Location after = scanner()->location();
      PreParserTraits::ReportMessageAt(before.beg_pos, after.end_pos,
                                       "strict_eval_arguments", NULL);
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

  Scanner::Location before = scanner()->peek_location();
  Expression expression = ParseLeftHandSideExpression(CHECK_OK);
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      Token::IsCountOp(peek())) {
    if (!scope_->is_classic_mode() &&
        expression.IsIdentifier() &&
        expression.AsIdentifier().IsEvalOrArguments()) {
      Scanner::Location after = scanner()->location();
      PreParserTraits::ReportMessageAt(before.beg_pos, after.end_pos,
                                       "strict_eval_arguments", NULL);
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

  Expression result = ParseMemberWithNewPrefixesExpression(CHECK_OK);

  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        Consume(Token::LBRACK);
        ParseExpression(true, CHECK_OK);
        Expect(Token::RBRACK, CHECK_OK);
        if (result.IsThis()) {
          result = Expression::ThisProperty();
        } else {
          result = Expression::Default();
        }
        break;
      }

      case Token::LPAREN: {
        ParseArguments(CHECK_OK);
        result = Expression::Default();
        break;
      }

      case Token::PERIOD: {
        Consume(Token::PERIOD);
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


PreParser::Expression PreParser::ParseMemberWithNewPrefixesExpression(
    bool* ok) {
  // NewExpression ::
  //   ('new')+ MemberExpression

  // See Parser::ParseNewExpression.

  if (peek() == Token::NEW) {
    Consume(Token::NEW);
    ParseMemberWithNewPrefixesExpression(CHECK_OK);
    if (peek() == Token::LPAREN) {
      // NewExpression with arguments.
      ParseArguments(CHECK_OK);
      // The expression can still continue with . or [ after the arguments.
      ParseMemberExpressionContinuation(Expression::Default(), CHECK_OK);
    }
    return Expression::Default();
  }
  // No 'new' keyword.
  return ParseMemberExpression(ok);
}


PreParser::Expression PreParser::ParseMemberExpression(bool* ok) {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments)*

  // The '[' Expression ']' and '.' Identifier parts are parsed by
  // ParseMemberExpressionContinuation, and the Arguments part is parsed by the
  // caller.

  // Parse the initial primary or function expression.
  Expression result = Expression::Default();
  if (peek() == Token::FUNCTION) {
    Consume(Token::FUNCTION);

    bool is_generator = allow_generators() && Check(Token::MUL);
    Identifier name = Identifier::Default();
    bool is_strict_reserved_name = false;
    Scanner::Location function_name_location = Scanner::Location::invalid();
    if (peek_any_identifier()) {
      name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved_name,
                                                 CHECK_OK);
      function_name_location = scanner()->location();
    }
    result = ParseFunctionLiteral(name,
                                  function_name_location,
                                  is_strict_reserved_name,
                                  is_generator,
                                  CHECK_OK);
  } else {
    result = ParsePrimaryExpression(CHECK_OK);
  }
  result = ParseMemberExpressionContinuation(result, CHECK_OK);
  return result;
}


PreParser::Expression PreParser::ParseMemberExpressionContinuation(
    PreParserExpression expression, bool* ok) {
  // Parses this part of MemberExpression:
  // ('[' Expression ']' | '.' Identifier)*
  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        Consume(Token::LBRACK);
        ParseExpression(true, CHECK_OK);
        Expect(Token::RBRACK, CHECK_OK);
        if (expression.IsThis()) {
          expression = Expression::ThisProperty();
        } else {
          expression = Expression::Default();
        }
        break;
      }
      case Token::PERIOD: {
        Consume(Token::PERIOD);
        ParseIdentifierName(CHECK_OK);
        if (expression.IsThis()) {
          expression = Expression::ThisProperty();
        } else {
          expression = Expression::Default();
        }
        break;
      }
      default:
        return expression;
    }
  }
  ASSERT(false);
  return PreParserExpression::Default();
}


PreParser::Expression PreParser::ParseArrayLiteral(bool* ok) {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'
  Expect(Token::LBRACK, CHECK_OK);
  while (peek() != Token::RBRACK) {
    if (peek() != Token::COMMA) {
      ParseAssignmentExpression(true, CHECK_OK);
    }
    if (peek() != Token::RBRACK) {
      Expect(Token::COMMA, CHECK_OK);
    }
  }
  Expect(Token::RBRACK, CHECK_OK);

  function_state_->NextMaterializedLiteralIndex();
  return Expression::Default();
}


PreParser::Expression PreParser::ParseObjectLiteral(bool* ok) {
  // ObjectLiteral ::
  //   '{' (
  //       ((IdentifierName | String | Number) ':' AssignmentExpression)
  //     | (('get' | 'set') (IdentifierName | String | Number) FunctionLiteral)
  //    )*[','] '}'

  ObjectLiteralChecker checker(this, scope_->language_mode());

  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    Token::Value next = peek();
    switch (next) {
      case Token::IDENTIFIER:
      case Token::FUTURE_RESERVED_WORD:
      case Token::FUTURE_STRICT_RESERVED_WORD: {
        bool is_getter = false;
        bool is_setter = false;
        ParseIdentifierNameOrGetOrSet(&is_getter, &is_setter, CHECK_OK);
        if ((is_getter || is_setter) && peek() != Token::COLON) {
            Token::Value name = Next();
            bool is_keyword = Token::IsKeyword(name);
            if (name != Token::IDENTIFIER &&
                name != Token::FUTURE_RESERVED_WORD &&
                name != Token::FUTURE_STRICT_RESERVED_WORD &&
                name != Token::NUMBER &&
                name != Token::STRING &&
                !is_keyword) {
              *ok = false;
              return Expression::Default();
            }
            if (!is_keyword) {
              LogSymbol();
            }
            PropertyKind type = is_getter ? kGetterProperty : kSetterProperty;
            checker.CheckProperty(name, type, CHECK_OK);
            ParseFunctionLiteral(Identifier::Default(),
                                 scanner()->location(),
                                 false,  // reserved words are allowed here
                                 false,  // not a generator
                                 CHECK_OK);
            if (peek() != Token::RBRACE) {
              Expect(Token::COMMA, CHECK_OK);
            }
            continue;  // restart the while
        }
        checker.CheckProperty(next, kValueProperty, CHECK_OK);
        break;
      }
      case Token::STRING:
        Consume(next);
        checker.CheckProperty(next, kValueProperty, CHECK_OK);
        LogSymbol();
        break;
      case Token::NUMBER:
        Consume(next);
        checker.CheckProperty(next, kValueProperty, CHECK_OK);
        break;
      default:
        if (Token::IsKeyword(next)) {
          Consume(next);
          checker.CheckProperty(next, kValueProperty, CHECK_OK);
        } else {
          // Unexpected token.
          *ok = false;
          return Expression::Default();
        }
    }

    Expect(Token::COLON, CHECK_OK);
    ParseAssignmentExpression(true, CHECK_OK);

    // TODO(1240767): Consider allowing trailing comma.
    if (peek() != Token::RBRACE) Expect(Token::COMMA, CHECK_OK);
  }
  Expect(Token::RBRACE, CHECK_OK);

  function_state_->NextMaterializedLiteralIndex();
  return Expression::Default();
}


PreParser::Arguments PreParser::ParseArguments(bool* ok) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  Expect(Token::LPAREN, ok);
  if (!*ok) return -1;
  bool done = (peek() == Token::RPAREN);
  int argc = 0;
  while (!done) {
    ParseAssignmentExpression(true, ok);
    if (!*ok) return -1;
    argc++;
    done = (peek() == Token::RPAREN);
    if (!done) {
      Expect(Token::COMMA, ok);
      if (!*ok) return -1;
    }
  }
  Expect(Token::RPAREN, ok);
  return argc;
}

PreParser::Expression PreParser::ParseFunctionLiteral(
    Identifier function_name,
    Scanner::Location function_name_location,
    bool name_is_strict_reserved,
    bool is_generator,
    bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  ScopeType outer_scope_type = scope_->type();
  bool inside_with = scope_->inside_with();
  PreParserScope function_scope(scope_, FUNCTION_SCOPE);
  FunctionState function_state(&function_state_, &scope_, &function_scope);
  function_state.set_is_generator(is_generator);
  //  FormalParameterList ::
  //    '(' (Identifier)*[','] ')'
  Expect(Token::LPAREN, CHECK_OK);
  int start_position = position();
  bool done = (peek() == Token::RPAREN);
  DuplicateFinder duplicate_finder(scanner()->unicode_cache());
  // We don't yet know if the function will be strict, so we cannot yet produce
  // errors for parameter names or duplicates. However, we remember the
  // locations of these errors if they occur and produce the errors later.
  Scanner::Location eval_args_error_loc = Scanner::Location::invalid();
  Scanner::Location dupe_error_loc = Scanner::Location::invalid();
  Scanner::Location reserved_error_loc = Scanner::Location::invalid();
  while (!done) {
    bool is_strict_reserved = false;
    Identifier param_name =
        ParseIdentifierOrStrictReservedWord(&is_strict_reserved, CHECK_OK);
    if (!eval_args_error_loc.IsValid() && param_name.IsEvalOrArguments()) {
      eval_args_error_loc = scanner()->location();
    }
    if (!reserved_error_loc.IsValid() && is_strict_reserved) {
      reserved_error_loc = scanner()->location();
    }

    int prev_value;
    if (scanner()->is_literal_ascii()) {
      prev_value =
          duplicate_finder.AddAsciiSymbol(scanner()->literal_ascii_string(), 1);
    } else {
      prev_value =
          duplicate_finder.AddUtf16Symbol(scanner()->literal_utf16_string(), 1);
    }

    if (!dupe_error_loc.IsValid() && prev_value != 0) {
      dupe_error_loc = scanner()->location();
    }

    done = (peek() == Token::RPAREN);
    if (!done) {
      Expect(Token::COMMA, CHECK_OK);
    }
  }
  Expect(Token::RPAREN, CHECK_OK);

  // Determine if the function will be lazily compiled.
  // Currently only happens to top-level functions.
  // Optimistically assume that all top-level functions are lazily compiled.
  bool is_lazily_compiled = (outer_scope_type == GLOBAL_SCOPE &&
                             !inside_with && allow_lazy() &&
                             !parenthesized_function_);
  parenthesized_function_ = false;

  Expect(Token::LBRACE, CHECK_OK);
  if (is_lazily_compiled) {
    ParseLazyFunctionLiteralBody(CHECK_OK);
  } else {
    ParseSourceElements(Token::RBRACE, ok);
  }
  Expect(Token::RBRACE, CHECK_OK);

  // Validate strict mode. We can do this only after parsing the function,
  // since the function can declare itself strict.
  if (!scope_->is_classic_mode()) {
    if (function_name.IsEvalOrArguments()) {
      ReportMessageAt(function_name_location, "strict_eval_arguments");
      *ok = false;
      return Expression::Default();
    }
    if (name_is_strict_reserved) {
      ReportMessageAt(function_name_location, "unexpected_strict_reserved");
      *ok = false;
      return Expression::Default();
    }
    if (eval_args_error_loc.IsValid()) {
      ReportMessageAt(eval_args_error_loc, "strict_eval_arguments");
      *ok = false;
      return Expression::Default();
    }
    if (dupe_error_loc.IsValid()) {
      ReportMessageAt(dupe_error_loc, "strict_param_dupe");
      *ok = false;
      return Expression::Default();
    }
    if (reserved_error_loc.IsValid()) {
      ReportMessageAt(reserved_error_loc, "unexpected_strict_reserved");
      *ok = false;
      return Expression::Default();
    }

    int end_position = scanner()->location().end_pos;
    CheckOctalLiteral(start_position, end_position, CHECK_OK);
    return Expression::StrictFunction();
  }

  return Expression::Default();
}


void PreParser::ParseLazyFunctionLiteralBody(bool* ok) {
  int body_start = position();
  log_->PauseRecording();
  ParseSourceElements(Token::RBRACE, ok);
  log_->ResumeRecording();
  if (!*ok) return;

  // Position right after terminal '}'.
  ASSERT_EQ(Token::RBRACE, scanner()->peek());
  int body_end = scanner()->peek_location().end_pos;
  log_->LogFunction(body_start, body_end,
                    function_state_->materialized_literal_count(),
                    function_state_->expected_property_count(),
                    scope_->language_mode());
}


PreParser::Expression PreParser::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments
  Expect(Token::MOD, CHECK_OK);
  if (!allow_natives_syntax()) {
    *ok = false;
    return Expression::Default();
  }
  // Allow "eval" or "arguments" for backward compatibility.
  ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
  ParseArguments(ok);

  return Expression::Default();
}

#undef CHECK_OK


void PreParser::LogSymbol() {
  int identifier_pos = position();
  if (scanner()->is_literal_ascii()) {
    log_->LogAsciiSymbol(identifier_pos, scanner()->literal_ascii_string());
  } else {
    log_->LogUtf16Symbol(identifier_pos, scanner()->literal_utf16_string());
  }
}


} }  // v8::internal
