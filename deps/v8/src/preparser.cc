// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

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

namespace v8 {
namespace internal {

void PreParserTraits::ReportMessageAt(Scanner::Location location,
                                      const char* message, const char* arg,
                                      ParseErrorType error_type) {
  ReportMessageAt(location.beg_pos, location.end_pos, message, arg, error_type);
}


void PreParserTraits::ReportMessageAt(int start_pos, int end_pos,
                                      const char* message, const char* arg,
                                      ParseErrorType error_type) {
  pre_parser_->log_->LogMessage(start_pos, end_pos, message, arg, error_type);
}


PreParserIdentifier PreParserTraits::GetSymbol(Scanner* scanner) {
  if (scanner->current_token() == Token::FUTURE_RESERVED_WORD) {
    return PreParserIdentifier::FutureReserved();
  } else if (scanner->current_token() ==
             Token::FUTURE_STRICT_RESERVED_WORD) {
    return PreParserIdentifier::FutureStrictReserved();
  } else if (scanner->current_token() == Token::LET) {
    return PreParserIdentifier::Let();
  } else if (scanner->current_token() == Token::STATIC) {
    return PreParserIdentifier::Static();
  } else if (scanner->current_token() == Token::YIELD) {
    return PreParserIdentifier::Yield();
  }
  if (scanner->UnescapedLiteralMatches("eval", 4)) {
    return PreParserIdentifier::Eval();
  }
  if (scanner->UnescapedLiteralMatches("arguments", 9)) {
    return PreParserIdentifier::Arguments();
  }
  if (scanner->LiteralMatches("prototype", 9)) {
    return PreParserIdentifier::Prototype();
  }
  if (scanner->LiteralMatches("constructor", 11)) {
    return PreParserIdentifier::Constructor();
  }
  return PreParserIdentifier::Default();
}


PreParserIdentifier PreParserTraits::GetNumberAsSymbol(Scanner* scanner) {
  return PreParserIdentifier::Default();
}


PreParserExpression PreParserTraits::ExpressionFromString(
    int pos, Scanner* scanner, PreParserFactory* factory) {
  if (scanner->UnescapedLiteralMatches("use strict", 10)) {
    return PreParserExpression::UseStrictStringLiteral();
  } else if (scanner->UnescapedLiteralMatches("use strong", 10)) {
    return PreParserExpression::UseStrongStringLiteral();
  }
  return PreParserExpression::StringLiteral();
}


PreParserExpression PreParserTraits::ParseV8Intrinsic(bool* ok) {
  return pre_parser_->ParseV8Intrinsic(ok);
}


PreParserExpression PreParserTraits::ParseFunctionLiteral(
    PreParserIdentifier name, Scanner::Location function_name_location,
    bool name_is_strict_reserved, FunctionKind kind,
    int function_token_position, FunctionLiteral::FunctionType type,
    FunctionLiteral::ArityRestriction arity_restriction, bool* ok) {
  return pre_parser_->ParseFunctionLiteral(
      name, function_name_location, name_is_strict_reserved, kind,
      function_token_position, type, arity_restriction, ok);
}


PreParser::PreParseResult PreParser::PreParseLazyFunction(
    LanguageMode language_mode, FunctionKind kind, ParserRecorder* log) {
  log_ = log;
  // Lazy functions always have trivial outer scopes (no with/catch scopes).
  Scope* top_scope = NewScope(scope_, SCRIPT_SCOPE);
  PreParserFactory top_factory(NULL);
  FunctionState top_state(&function_state_, &scope_, top_scope, kNormalFunction,
                          &top_factory);
  scope_->SetLanguageMode(language_mode);
  Scope* function_scope = NewScope(scope_, FUNCTION_SCOPE);
  PreParserFactory function_factory(NULL);
  FunctionState function_state(&function_state_, &scope_, function_scope, kind,
                               &function_factory);
  DCHECK_EQ(Token::LBRACE, scanner()->current_token());
  bool ok = true;
  int start_position = peek_position();
  ParseLazyFunctionLiteralBody(&ok);
  if (stack_overflow()) return kPreParseStackOverflow;
  if (!ok) {
    ReportUnexpectedToken(scanner()->current_token());
  } else {
    DCHECK_EQ(Token::RBRACE, scanner()->peek());
    if (is_strict(scope_->language_mode())) {
      int end_pos = scanner()->location().end_pos;
      CheckStrictOctalLiteral(start_position, end_pos, &ok);
    }
  }
  return kPreParseSuccess;
}


PreParserExpression PreParserTraits::ParseClassLiteral(
    PreParserIdentifier name, Scanner::Location class_name_location,
    bool name_is_strict_reserved, int pos, bool* ok) {
  return pre_parser_->ParseClassLiteral(name, class_name_location,
                                        name_is_strict_reserved, pos, ok);
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


PreParser::Statement PreParser::ParseStatementListItem(bool* ok) {
  // ECMA 262 6th Edition
  // StatementListItem[Yield, Return] :
  //   Statement[?Yield, ?Return]
  //   Declaration[?Yield]
  //
  // Declaration[Yield] :
  //   HoistableDeclaration[?Yield]
  //   ClassDeclaration[?Yield]
  //   LexicalDeclaration[In, ?Yield]
  //
  // HoistableDeclaration[Yield, Default] :
  //   FunctionDeclaration[?Yield, ?Default]
  //   GeneratorDeclaration[?Yield, ?Default]
  //
  // LexicalDeclaration[In, Yield] :
  //   LetOrConst BindingList[?In, ?Yield] ;

  switch (peek()) {
    case Token::FUNCTION:
      return ParseFunctionDeclaration(ok);
    case Token::CLASS:
      return ParseClassDeclaration(ok);
    case Token::CONST:
      return ParseVariableStatement(kStatementListItem, ok);
    case Token::LET:
      if (is_strict(language_mode())) {
        return ParseVariableStatement(kStatementListItem, ok);
      }
      // Fall through.
    default:
      return ParseStatement(ok);
  }
}


void PreParser::ParseStatementList(int end_token, bool* ok) {
  // SourceElements ::
  //   (Statement)* <end_token>

  bool directive_prologue = true;
  while (peek() != end_token) {
    if (directive_prologue && peek() != Token::STRING) {
      directive_prologue = false;
    }
    Token::Value token = peek();
    Scanner::Location old_super_loc = function_state_->super_call_location();
    Statement statement = ParseStatementListItem(ok);
    if (!*ok) return;
    Scanner::Location super_loc = function_state_->super_call_location();
    if (is_strong(language_mode()) &&
        i::IsConstructor(function_state_->kind()) &&
        !old_super_loc.IsValid() && super_loc.IsValid() &&
        token != Token::SUPER) {
      ReportMessageAt(super_loc, "strong_super_call_nested");
      *ok = false;
      return;
    }
    if (directive_prologue) {
      if (statement.IsUseStrictLiteral()) {
        scope_->SetLanguageMode(
            static_cast<LanguageMode>(scope_->language_mode() | STRICT_BIT));
      } else if (statement.IsUseStrongLiteral() && allow_strong_mode()) {
        scope_->SetLanguageMode(static_cast<LanguageMode>(
            scope_->language_mode() | STRICT_BIT | STRONG_BIT));
      } else if (!statement.IsStringLiteral()) {
        directive_prologue = false;
      }
    }
  }
}


#define CHECK_OK  ok);                   \
  if (!*ok) return Statement::Default();  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY


PreParser::Statement PreParser::ParseStatement(bool* ok) {
  // Statement ::
  //   EmptyStatement
  //   ...
  return ParseSubStatement(ok);
}


PreParser::Statement PreParser::ParseSubStatement(bool* ok) {
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
      if (is_strong(language_mode())) {
        PreParserTraits::ReportMessageAt(scanner()->peek_location(),
                                         "strong_empty");
        *ok = false;
        return Statement::Default();
      }
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
      if (is_strict(language_mode())) {
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
      return ParseVariableStatement(kStatement, ok);

    case Token::CONST:
      // In ES6 CONST is not allowed as a Statement, only as a
      // LexicalDeclaration, however we continue to allow it in sloppy mode for
      // backwards compatibility.
      if (is_sloppy(language_mode())) {
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
  bool is_generator = Check(Token::MUL);
  bool is_strict_reserved = false;
  Identifier name = ParseIdentifierOrStrictReservedWord(
      &is_strict_reserved, CHECK_OK);
  ParseFunctionLiteral(name, scanner()->location(), is_strict_reserved,
                       is_generator ? FunctionKind::kGeneratorFunction
                                    : FunctionKind::kNormalFunction,
                       pos, FunctionLiteral::DECLARATION,
                       FunctionLiteral::NORMAL_ARITY, CHECK_OK);
  return Statement::FunctionDeclaration();
}


PreParser::Statement PreParser::ParseClassDeclaration(bool* ok) {
  Expect(Token::CLASS, CHECK_OK);
  if (!allow_harmony_sloppy() && is_sloppy(language_mode())) {
    ReportMessage("sloppy_lexical");
    *ok = false;
    return Statement::Default();
  }

  int pos = position();
  bool is_strict_reserved = false;
  Identifier name =
      ParseIdentifierOrStrictReservedWord(&is_strict_reserved, CHECK_OK);
  ParseClassLiteral(name, scanner()->location(), is_strict_reserved, pos,
                    CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseBlock(bool* ok) {
  // Block ::
  //   '{' Statement* '}'

  // Note that a Block does not introduce a new execution scope!
  // (ECMA-262, 3rd, 12.2)
  //
  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    if (is_strict(language_mode())) {
      ParseStatementListItem(CHECK_OK);
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
  bool is_strict_const = false;
  if (peek() == Token::VAR) {
    if (is_strong(language_mode())) {
      Scanner::Location location = scanner()->peek_location();
      ReportMessageAt(location, "strong_var");
      *ok = false;
      return Statement::Default();
    }
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
    if (is_strict(language_mode())) {
      DCHECK(var_context != kStatement);
      is_strict_const = true;
      require_initializer = var_context != kForStatement;
    }
  } else if (peek() == Token::LET && is_strict(language_mode())) {
    Consume(Token::LET);
    DCHECK(var_context != kStatement);
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
    if (peek() == Token::ASSIGN || require_initializer ||
        // require initializers for multiple consts.
        (is_strict_const && peek() == Token::COMMA)) {
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

  switch (peek()) {
    case Token::FUNCTION:
    case Token::LBRACE:
      UNREACHABLE();  // Always handled by the callers.
    case Token::CLASS:
      ReportUnexpectedToken(Next());
      *ok = false;
      return Statement::Default();

    // TODO(arv): Handle `let [`
    // https://code.google.com/p/v8/issues/detail?id=3847

    default:
      break;
  }

  bool starts_with_identifier = peek_any_identifier();
  Expression expr = ParseExpression(true, CHECK_OK);
  // Even if the expression starts with an identifier, it is not necessarily an
  // identifier. For example, "foo + bar" starts with an identifier but is not
  // an identifier.
  if (starts_with_identifier && expr.IsIdentifier() && peek() == Token::COLON) {
    // Expression is a single identifier, and not, e.g., a parenthesized
    // identifier.
    DCHECK(!expr.AsIdentifier().IsFutureReserved());
    DCHECK(is_sloppy(language_mode()) ||
           !IsFutureStrictReserved(expr.AsIdentifier()));
    Consume(Token::COLON);
    return ParseStatement(ok);
    // Preparsing is disabled for extensions (because the extension details
    // aren't passed to lazily compiled functions), so we don't
    // accept "native function" in the preparser.
  }
  // Parsed expression statement.
  // Detect attempts at 'let' declarations in sloppy mode.
  if (peek() == Token::IDENTIFIER && is_sloppy(language_mode()) &&
      expr.IsIdentifier() && expr.AsIdentifier().IsLet()) {
    ReportMessage("sloppy_lexical", NULL);
    *ok = false;
    return Statement::Default();
  }
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
  ParseSubStatement(CHECK_OK);
  if (peek() == Token::ELSE) {
    Next();
    ParseSubStatement(CHECK_OK);
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
  function_state_->set_return_location(scanner()->location());

  // An ECMAScript program is considered syntactically incorrect if it
  // contains a return statement that is not within the body of a
  // function. See ECMA-262, section 12.9, page 67.
  // This is not handled during preparsing.

  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      tok != Token::SEMICOLON &&
      tok != Token::RBRACE &&
      tok != Token::EOS) {
    if (is_strong(language_mode()) &&
        i::IsConstructor(function_state_->kind())) {
      int pos = peek_position();
      ReportMessageAt(Scanner::Location(pos, pos + 1),
                      "strong_constructor_return_value");
      *ok = false;
      return Statement::Default();
    }
    ParseExpression(true, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseWithStatement(bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement
  Expect(Token::WITH, CHECK_OK);
  if (is_strict(language_mode())) {
    ReportMessageAt(scanner()->location(), "strict_mode_with");
    *ok = false;
    return Statement::Default();
  }
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  Scope* with_scope = NewScope(scope_, WITH_SCOPE);
  BlockState block_state(&scope_, with_scope);
  ParseSubStatement(CHECK_OK);
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
      ParseStatementListItem(CHECK_OK);
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
  ParseSubStatement(CHECK_OK);
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
  ParseSubStatement(ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseForStatement(bool* ok) {
  // ForStatement ::
  //   'for' '(' Expression? ';' Expression? ';' Expression? ')' Statement

  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  bool is_let_identifier_expression = false;
  if (peek() != Token::SEMICOLON) {
    ForEachStatement::VisitMode visit_mode;
    if (peek() == Token::VAR || peek() == Token::CONST ||
        (peek() == Token::LET && is_strict(language_mode()))) {
      bool is_lexical = peek() == Token::LET ||
                        (peek() == Token::CONST && is_strict(language_mode()));
      int decl_count;
      VariableDeclarationProperties decl_props = kHasNoInitializers;
      ParseVariableDeclarations(
          kForStatement, &decl_props, &decl_count, CHECK_OK);
      bool has_initializers = decl_props == kHasInitializers;
      bool accept_IN = decl_count == 1 && !(is_lexical && has_initializers);
      bool accept_OF = !has_initializers;
      if (accept_IN && CheckInOrOf(accept_OF, &visit_mode, ok)) {
        if (!*ok) return Statement::Default();
        ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);
        ParseSubStatement(CHECK_OK);
        return Statement::Default();
      }
    } else {
      Expression lhs = ParseExpression(false, CHECK_OK);
      is_let_identifier_expression =
          lhs.IsIdentifier() && lhs.AsIdentifier().IsLet();
      if (CheckInOrOf(lhs.IsIdentifier(), &visit_mode, ok)) {
        if (!*ok) return Statement::Default();
        ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);
        ParseSubStatement(CHECK_OK);
        return Statement::Default();
      }
    }
  }

  // Parsed initializer at this point.
  // Detect attempts at 'let' declarations in sloppy mode.
  if (peek() == Token::IDENTIFIER && is_sloppy(language_mode()) &&
      is_let_identifier_expression) {
    ReportMessage("sloppy_lexical", NULL);
    *ok = false;
    return Statement::Default();
  }
  Expect(Token::SEMICOLON, CHECK_OK);

  if (peek() != Token::SEMICOLON) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(Token::SEMICOLON, CHECK_OK);

  if (peek() != Token::RPAREN) {
    ParseExpression(true, CHECK_OK);
  }
  Expect(Token::RPAREN, CHECK_OK);

  ParseSubStatement(ok);
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
      Scope* with_scope = NewScope(scope_, WITH_SCOPE);
      BlockState block_state(&scope_, with_scope);
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
    Identifier function_name, Scanner::Location function_name_location,
    bool name_is_strict_reserved, FunctionKind kind, int function_token_pos,
    FunctionLiteral::FunctionType function_type,
    FunctionLiteral::ArityRestriction arity_restriction, bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  bool outer_is_script_scope = scope_->is_script_scope();
  Scope* function_scope = NewScope(scope_, FUNCTION_SCOPE);
  PreParserFactory factory(NULL);
  FunctionState function_state(&function_state_, &scope_, function_scope, kind,
                               &factory);
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

  bool is_rest = false;
  bool done = arity_restriction == FunctionLiteral::GETTER_ARITY ||
      (peek() == Token::RPAREN &&
       arity_restriction != FunctionLiteral::SETTER_ARITY);
  while (!done) {
    bool is_strict_reserved = false;
    is_rest = peek() == Token::ELLIPSIS && allow_harmony_rest_params();
    if (is_rest) {
      Consume(Token::ELLIPSIS);
    }

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
    if (!done) {
      if (is_rest) {
        ReportMessageAt(scanner()->peek_location(), "param_after_rest");
        *ok = false;
        return Expression::Default();
      }
      Expect(Token::COMMA, CHECK_OK);
    }
  }
  Expect(Token::RPAREN, CHECK_OK);

  // See Parser::ParseFunctionLiteral for more information about lazy parsing
  // and lazy compilation.
  bool is_lazily_parsed =
      (outer_is_script_scope && allow_lazy() && !parenthesized_function_);
  parenthesized_function_ = false;

  Expect(Token::LBRACE, CHECK_OK);
  if (is_lazily_parsed) {
    ParseLazyFunctionLiteralBody(CHECK_OK);
  } else {
    ParseStatementList(Token::RBRACE, CHECK_OK);
  }
  Expect(Token::RBRACE, CHECK_OK);

  // Validate name and parameter names. We can do this only after parsing the
  // function, since the function can declare itself strict.
  CheckFunctionName(language_mode(), kind, function_name,
                    name_is_strict_reserved, function_name_location, CHECK_OK);
  const bool use_strict_params = is_rest || IsConciseMethod(kind);
  CheckFunctionParameterNames(language_mode(), use_strict_params,
                              eval_args_error_loc, dupe_error_loc,
                              reserved_error_loc, CHECK_OK);

  if (is_strict(language_mode())) {
    int end_position = scanner()->location().end_pos;
    CheckStrictOctalLiteral(start_position, end_position, CHECK_OK);
  }

  if (is_strong(language_mode()) && IsSubclassConstructor(kind)) {
    if (!function_state.super_call_location().IsValid()) {
      ReportMessageAt(function_name_location, "strong_super_call_missing",
                      kReferenceError);
      *ok = false;
      return Expression::Default();
    }
  }

  return Expression::Default();
}


void PreParser::ParseLazyFunctionLiteralBody(bool* ok) {
  int body_start = position();
  ParseStatementList(Token::RBRACE, ok);
  if (!*ok) return;

  // Position right after terminal '}'.
  DCHECK_EQ(Token::RBRACE, scanner()->peek());
  int body_end = scanner()->peek_location().end_pos;
  log_->LogFunction(body_start, body_end,
                    function_state_->materialized_literal_count(),
                    function_state_->expected_property_count(), language_mode(),
                    scope_->uses_super_property());
}


PreParserExpression PreParser::ParseClassLiteral(
    PreParserIdentifier name, Scanner::Location class_name_location,
    bool name_is_strict_reserved, int pos, bool* ok) {
  // All parts of a ClassDeclaration and ClassExpression are strict code.
  if (name_is_strict_reserved) {
    ReportMessageAt(class_name_location, "unexpected_strict_reserved");
    *ok = false;
    return EmptyExpression();
  }
  if (IsEvalOrArguments(name)) {
    ReportMessageAt(class_name_location, "strict_eval_arguments");
    *ok = false;
    return EmptyExpression();
  }

  Scope* scope = NewScope(scope_, BLOCK_SCOPE);
  BlockState block_state(&scope_, scope);
  scope_->SetLanguageMode(
      static_cast<LanguageMode>(scope_->language_mode() | STRICT_BIT));
  // TODO(marja): Make PreParser use scope names too.
  // scope_->SetScopeName(name);

  bool has_extends = Check(Token::EXTENDS);
  if (has_extends) {
    ParseLeftHandSideExpression(CHECK_OK);
  }

  ClassLiteralChecker checker(this);
  bool has_seen_constructor = false;

  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    if (Check(Token::SEMICOLON)) continue;
    const bool in_class = true;
    const bool is_static = false;
    bool is_computed_name = false;  // Classes do not care about computed
                                    // property names here.
    ParsePropertyDefinition(&checker, in_class, has_extends, is_static,
                            &is_computed_name, &has_seen_constructor, CHECK_OK);
  }

  Expect(Token::RBRACE, CHECK_OK);

  return Expression::Default();
}


PreParser::Expression PreParser::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments
  Expect(Token::MOD, CHECK_OK);
  if (!allow_natives()) {
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
