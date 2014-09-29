// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "include/v8stdint.h"

#include "src/allocation.h"
#include "src/base/logging.h"
#include "src/conversions-inl.h"
#include "src/conversions.h"
#include "src/globals.h"
#include "src/hashmap.h"
#include "src/list.h"
#include "src/preparse-data.h"
#include "src/preparse-data-format.h"
#include "src/preparser.h"
#include "src/unicode.h"
#include "src/utils.h"

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
                                      const char* arg,
                                      bool is_reference_error) {
  ReportMessageAt(location.beg_pos,
                  location.end_pos,
                  message,
                  arg,
                  is_reference_error);
}


void PreParserTraits::ReportMessageAt(int start_pos,
                                      int end_pos,
                                      const char* message,
                                      const char* arg,
                                      bool is_reference_error) {
  pre_parser_->log_->LogMessage(start_pos, end_pos, message, arg,
                                is_reference_error);
}


PreParserIdentifier PreParserTraits::GetSymbol(Scanner* scanner) {
  if (scanner->current_token() == Token::FUTURE_RESERVED_WORD) {
    return PreParserIdentifier::FutureReserved();
  } else if (scanner->current_token() ==
             Token::FUTURE_STRICT_RESERVED_WORD) {
    return PreParserIdentifier::FutureStrictReserved();
  } else if (scanner->current_token() == Token::LET) {
    return PreParserIdentifier::Let();
  } else if (scanner->current_token() == Token::YIELD) {
    return PreParserIdentifier::Yield();
  }
  if (scanner->UnescapedLiteralMatches("eval", 4)) {
    return PreParserIdentifier::Eval();
  }
  if (scanner->UnescapedLiteralMatches("arguments", 9)) {
    return PreParserIdentifier::Arguments();
  }
  return PreParserIdentifier::Default();
}


PreParserExpression PreParserTraits::ExpressionFromString(
    int pos, Scanner* scanner, PreParserFactory* factory) {
  if (scanner->UnescapedLiteralMatches("use strict", 10)) {
    return PreParserExpression::UseStrictStringLiteral();
  }
  return PreParserExpression::StringLiteral();
}


PreParserExpression PreParserTraits::ParseV8Intrinsic(bool* ok) {
  return pre_parser_->ParseV8Intrinsic(ok);
}


PreParserExpression PreParserTraits::ParseFunctionLiteral(
    PreParserIdentifier name,
    Scanner::Location function_name_location,
    bool name_is_strict_reserved,
    bool is_generator,
    int function_token_position,
    FunctionLiteral::FunctionType type,
    FunctionLiteral::ArityRestriction arity_restriction,
    bool* ok) {
  return pre_parser_->ParseFunctionLiteral(
      name, function_name_location, name_is_strict_reserved, is_generator,
      function_token_position, type, arity_restriction, ok);
}


PreParser::PreParseResult PreParser::PreParseLazyFunction(
    StrictMode strict_mode, bool is_generator, ParserRecorder* log) {
  log_ = log;
  // Lazy functions always have trivial outer scopes (no with/catch scopes).
  PreParserScope top_scope(scope_, GLOBAL_SCOPE);
  FunctionState top_state(&function_state_, &scope_, &top_scope, NULL,
                          this->ast_value_factory());
  scope_->SetStrictMode(strict_mode);
  PreParserScope function_scope(scope_, FUNCTION_SCOPE);
  FunctionState function_state(&function_state_, &scope_, &function_scope, NULL,
                               this->ast_value_factory());
  function_state.set_is_generator(is_generator);
  DCHECK_EQ(Token::LBRACE, scanner()->current_token());
  bool ok = true;
  int start_position = peek_position();
  ParseLazyFunctionLiteralBody(&ok);
  if (stack_overflow()) return kPreParseStackOverflow;
  if (!ok) {
    ReportUnexpectedToken(scanner()->current_token());
  } else {
    DCHECK_EQ(Token::RBRACE, scanner()->peek());
    if (scope_->strict_mode() == STRICT) {
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
    case Token::CONST:
      return ParseVariableStatement(kSourceElement, ok);
    case Token::LET:
      DCHECK(allow_harmony_scoping());
      if (strict_mode() == STRICT) {
        return ParseVariableStatement(kSourceElement, ok);
      }
      // Fall through.
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
        scope_->SetStrictMode(STRICT);
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
      if (strict_mode() == STRICT) {
        PreParserTraits::ReportMessageAt(start_location.beg_pos,
                                         end_location.end_pos,
                                         "strict_function");
        *ok = false;
        return Statement::Default();
      } else {
        return statement;
      }
    }

    case Token::DEBUGGER:
      return ParseDebuggerStatement(ok);

    case Token::VAR:
    case Token::CONST:
      return ParseVariableStatement(kStatement, ok);

    case Token::LET:
      DCHECK(allow_harmony_scoping());
      if (strict_mode() == STRICT) {
        return ParseVariableStatement(kStatement, ok);
      }
      // Fall through.
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
  int pos = position();
  bool is_generator = allow_generators() && Check(Token::MUL);
  bool is_strict_reserved = false;
  Identifier name = ParseIdentifierOrStrictReservedWord(
      &is_strict_reserved, CHECK_OK);
  ParseFunctionLiteral(name,
                       scanner()->location(),
                       is_strict_reserved,
                       is_generator,
                       pos,
                       FunctionLiteral::DECLARATION,
                       FunctionLiteral::NORMAL_ARITY,
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
    if (allow_harmony_scoping() && strict_mode() == STRICT) {
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
    // However disallowing const in sloppy mode will break compatibility with
    // existing pages. Therefore we keep allowing const with the old
    // non-harmony semantics in sloppy mode.
    Consume(Token::CONST);
    if (strict_mode() == STRICT) {
      if (allow_harmony_scoping()) {
        if (var_context != kSourceElement && var_context != kForStatement) {
          ReportMessageAt(scanner()->peek_location(), "unprotected_const");
          *ok = false;
          return Statement::Default();
        }
        require_initializer = true;
      } else {
        Scanner::Location location = scanner()->peek_location();
        ReportMessageAt(location, "strict_const");
        *ok = false;
        return Statement::Default();
      }
    }
  } else if (peek() == Token::LET && strict_mode() == STRICT) {
    Consume(Token::LET);
    if (var_context != kSourceElement && var_context != kForStatement) {
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
    DCHECK(!expr.AsIdentifier().IsFutureReserved());
    DCHECK(strict_mode() == SLOPPY ||
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

  // Consume the return token. It is necessary to do before
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
  if (strict_mode() == STRICT) {
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
      (accept_OF && CheckContextualKeyword(CStrVector("of")))) {
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
        (peek() == Token::LET && strict_mode() == STRICT)) {
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


PreParser::Expression PreParser::ParseFunctionLiteral(
    Identifier function_name,
    Scanner::Location function_name_location,
    bool name_is_strict_reserved,
    bool is_generator,
    int function_token_pos,
    FunctionLiteral::FunctionType function_type,
    FunctionLiteral::ArityRestriction arity_restriction,
    bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  ScopeType outer_scope_type = scope_->type();
  PreParserScope function_scope(scope_, FUNCTION_SCOPE);
  FunctionState function_state(&function_state_, &scope_, &function_scope, NULL,
                               this->ast_value_factory());
  function_state.set_is_generator(is_generator);
  //  FormalParameterList ::
  //    '(' (Identifier)*[','] ')'
  Expect(Token::LPAREN, CHECK_OK);
  int start_position = position();
  DuplicateFinder duplicate_finder(scanner()->unicode_cache());
  // We don't yet know if the function will be strict, so we cannot yet produce
  // errors for parameter names or duplicates. However, we remember the
  // locations of these errors if they occur and produce the errors later.
  Scanner::Location eval_args_error_loc = Scanner::Location::invalid();
  Scanner::Location dupe_error_loc = Scanner::Location::invalid();
  Scanner::Location reserved_error_loc = Scanner::Location::invalid();

  bool done = arity_restriction == FunctionLiteral::GETTER_ARITY ||
      (peek() == Token::RPAREN &&
       arity_restriction != FunctionLiteral::SETTER_ARITY);
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

    int prev_value = scanner()->FindSymbol(&duplicate_finder, 1);

    if (!dupe_error_loc.IsValid() && prev_value != 0) {
      dupe_error_loc = scanner()->location();
    }

    if (arity_restriction == FunctionLiteral::SETTER_ARITY) break;
    done = (peek() == Token::RPAREN);
    if (!done) Expect(Token::COMMA, CHECK_OK);
  }
  Expect(Token::RPAREN, CHECK_OK);

  // See Parser::ParseFunctionLiteral for more information about lazy parsing
  // and lazy compilation.
  bool is_lazily_parsed = (outer_scope_type == GLOBAL_SCOPE && allow_lazy() &&
                           !parenthesized_function_);
  parenthesized_function_ = false;

  Expect(Token::LBRACE, CHECK_OK);
  if (is_lazily_parsed) {
    ParseLazyFunctionLiteralBody(CHECK_OK);
  } else {
    ParseSourceElements(Token::RBRACE, ok);
  }
  Expect(Token::RBRACE, CHECK_OK);

  // Validate strict mode. We can do this only after parsing the function,
  // since the function can declare itself strict.
  if (strict_mode() == STRICT) {
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
  }

  return Expression::Default();
}


void PreParser::ParseLazyFunctionLiteralBody(bool* ok) {
  int body_start = position();
  ParseSourceElements(Token::RBRACE, ok);
  if (!*ok) return;

  // Position right after terminal '}'.
  DCHECK_EQ(Token::RBRACE, scanner()->peek());
  int body_end = scanner()->peek_location().end_pos;
  log_->LogFunction(body_start, body_end,
                    function_state_->materialized_literal_count(),
                    function_state_->expected_property_count(),
                    strict_mode());
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


} }  // v8::internal
