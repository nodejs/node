// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "src/allocation.h"
#include "src/base/logging.h"
#include "src/conversions-inl.h"
#include "src/conversions.h"
#include "src/globals.h"
#include "src/list.h"
#include "src/parsing/parser-base.h"
#include "src/parsing/preparse-data-format.h"
#include "src/parsing/preparse-data.h"
#include "src/parsing/preparser.h"
#include "src/unicode.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// The CHECK_OK macro is a convenient macro to enforce error
// handling for functions that may fail (by returning !*ok).
//
// CAUTION: This macro appends extra statements after a call,
// thus it must never be used where only a single statement
// is correct (e.g. an if statement branch w/o braces)!

#define CHECK_OK  ok);                   \
  if (!*ok) return Statement::Default(); \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

// Used in functions where the return type is not ExpressionT.
#define CHECK_OK_CUSTOM(x) ok); \
  if (!*ok) return this->x();   \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

PreParserIdentifier PreParser::GetSymbol() const {
  switch (scanner()->current_token()) {
    case Token::ENUM:
      return PreParserIdentifier::Enum();
    case Token::AWAIT:
      return PreParserIdentifier::Await();
    case Token::FUTURE_STRICT_RESERVED_WORD:
      return PreParserIdentifier::FutureStrictReserved();
    case Token::LET:
      return PreParserIdentifier::Let();
    case Token::STATIC:
      return PreParserIdentifier::Static();
    case Token::YIELD:
      return PreParserIdentifier::Yield();
    case Token::ASYNC:
      return PreParserIdentifier::Async();
    default:
      if (scanner()->UnescapedLiteralMatches("eval", 4))
        return PreParserIdentifier::Eval();
      if (scanner()->UnescapedLiteralMatches("arguments", 9))
        return PreParserIdentifier::Arguments();
      if (scanner()->UnescapedLiteralMatches("undefined", 9))
        return PreParserIdentifier::Undefined();
      if (scanner()->LiteralMatches("prototype", 9))
        return PreParserIdentifier::Prototype();
      if (scanner()->LiteralMatches("constructor", 11))
        return PreParserIdentifier::Constructor();
      return PreParserIdentifier::Default();
  }
}

PreParser::PreParseResult PreParser::PreParseLazyFunction(
    LanguageMode language_mode, FunctionKind kind, bool has_simple_parameters,
    bool parsing_module, ParserRecorder* log, Scanner::BookmarkScope* bookmark,
    int* use_counts) {
  parsing_module_ = parsing_module;
  log_ = log;
  use_counts_ = use_counts;
  // Lazy functions always have trivial outer scopes (no with/catch scopes).
  DCHECK_NULL(scope_state_);
  DeclarationScope* top_scope = NewScriptScope();
  FunctionState top_state(&function_state_, &scope_state_, top_scope,
                          kNormalFunction);
  scope()->SetLanguageMode(language_mode);
  DeclarationScope* function_scope = NewFunctionScope(kind);
  if (!has_simple_parameters) function_scope->SetHasNonSimpleParameters();
  FunctionState function_state(&function_state_, &scope_state_, function_scope,
                               kind);
  DCHECK_EQ(Token::LBRACE, scanner()->current_token());
  bool ok = true;
  int start_position = peek_position();
  ParseLazyFunctionLiteralBody(&ok, bookmark);
  use_counts_ = nullptr;
  if (bookmark && bookmark->HasBeenReset()) {
    // Do nothing, as we've just aborted scanning this function.
  } else if (stack_overflow()) {
    return kPreParseStackOverflow;
  } else if (!ok) {
    ReportUnexpectedToken(scanner()->current_token());
  } else {
    DCHECK_EQ(Token::RBRACE, scanner()->peek());
    if (is_strict(scope()->language_mode())) {
      int end_pos = scanner()->location().end_pos;
      CheckStrictOctalLiteral(start_position, end_pos, &ok);
      CheckDecimalLiteralWithLeadingZero(use_counts, start_position, end_pos);
      if (!ok) return kPreParseSuccess;
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
      return ParseHoistableDeclaration(ok);
    case Token::CLASS:
      return ParseClassDeclaration(ok);
    case Token::CONST:
      return ParseVariableStatement(kStatementListItem, ok);
    case Token::LET:
      if (IsNextLetKeyword()) {
        return ParseVariableStatement(kStatementListItem, ok);
      }
      break;
    case Token::ASYNC:
      if (allow_harmony_async_await() && PeekAhead() == Token::FUNCTION &&
          !scanner()->HasAnyLineTerminatorAfterNext()) {
        Consume(Token::ASYNC);
        return ParseAsyncFunctionDeclaration(ok);
      }
    /* falls through */
    default:
      break;
  }
  return ParseStatement(kAllowLabelledFunctionStatement, ok);
}


void PreParser::ParseStatementList(int end_token, bool* ok,
                                   Scanner::BookmarkScope* bookmark) {
  // SourceElements ::
  //   (Statement)* <end_token>

  // Bookkeeping for trial parse if bookmark is set:
  DCHECK_IMPLIES(bookmark, bookmark->HasBeenSet());
  bool maybe_reset = bookmark != nullptr;
  int count_statements = 0;

  bool directive_prologue = true;
  while (peek() != end_token) {
    if (directive_prologue && peek() != Token::STRING) {
      directive_prologue = false;
    }
    bool starts_with_identifier = peek() == Token::IDENTIFIER;
    Scanner::Location token_loc = scanner()->peek_location();
    Statement statement = ParseStatementListItem(CHECK_OK_CUSTOM(Void));

    if (directive_prologue) {
      bool use_strict_found = statement.IsUseStrictLiteral();

      if (use_strict_found) {
        scope()->SetLanguageMode(
            static_cast<LanguageMode>(scope()->language_mode() | STRICT));
      } else if (!statement.IsStringLiteral()) {
        directive_prologue = false;
      }

      if (use_strict_found && !scope()->HasSimpleParameters()) {
        // TC39 deemed "use strict" directives to be an error when occurring
        // in the body of a function with non-simple parameter list, on
        // 29/7/2015. https://goo.gl/ueA7Ln
        ReportMessageAt(token_loc,
                        MessageTemplate::kIllegalLanguageModeDirective,
                        "use strict");
        *ok = false;
        return;
      }
    }

    // If we're allowed to reset to a bookmark, we will do so when we see a long
    // and trivial function.
    // Our current definition of 'long and trivial' is:
    // - over 200 statements
    // - all starting with an identifier (i.e., no if, for, while, etc.)
    if (maybe_reset && (!starts_with_identifier ||
                        ++count_statements > kLazyParseTrialLimit)) {
      if (count_statements > kLazyParseTrialLimit) {
        bookmark->Reset();
        return;
      }
      maybe_reset = false;
    }
  }
}


PreParser::Statement PreParser::ParseStatement(
    AllowLabelledFunctionStatement allow_function, bool* ok) {
  // Statement ::
  //   EmptyStatement
  //   ...

  if (peek() == Token::SEMICOLON) {
    Next();
    return Statement::Default();
  }
  return ParseSubStatement(allow_function, ok);
}

PreParser::Statement PreParser::ParseScopedStatement(bool legacy, bool* ok) {
  if (is_strict(language_mode()) || peek() != Token::FUNCTION ||
      (legacy && allow_harmony_restrictive_declarations())) {
    return ParseSubStatement(kDisallowLabelledFunctionStatement, ok);
  } else {
    BlockState block_state(&scope_state_);
    return ParseFunctionDeclaration(ok);
  }
}

PreParser::Statement PreParser::ParseSubStatement(
    AllowLabelledFunctionStatement allow_function, bool* ok) {
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

    case Token::FUNCTION:
      // FunctionDeclaration only allowed as a StatementListItem, not in
      // an arbitrary Statement position. Exceptions such as
      // ES#sec-functiondeclarations-in-ifstatement-statement-clauses
      // are handled by calling ParseScopedStatement rather than
      // ParseSubStatement directly.
      ReportMessageAt(scanner()->peek_location(),
                      is_strict(language_mode())
                          ? MessageTemplate::kStrictFunction
                          : MessageTemplate::kSloppyFunction);
      *ok = false;
      return Statement::Default();

    case Token::DEBUGGER:
      return ParseDebuggerStatement(ok);

    case Token::VAR:
      return ParseVariableStatement(kStatement, ok);

    default:
      return ParseExpressionOrLabelledStatement(allow_function, ok);
  }
}

PreParser::Statement PreParser::ParseHoistableDeclaration(
    int pos, ParseFunctionFlags flags, bool* ok) {
  const bool is_generator = flags & ParseFunctionFlags::kIsGenerator;
  const bool is_async = flags & ParseFunctionFlags::kIsAsync;
  DCHECK(!is_generator || !is_async);

  bool is_strict_reserved = false;
  Identifier name = ParseIdentifierOrStrictReservedWord(
      &is_strict_reserved, CHECK_OK);

  ParseFunctionLiteral(name, scanner()->location(),
                       is_strict_reserved ? kFunctionNameIsStrictReserved
                                          : kFunctionNameValidityUnknown,
                       is_generator ? FunctionKind::kGeneratorFunction
                                    : is_async ? FunctionKind::kAsyncFunction
                                               : FunctionKind::kNormalFunction,
                       pos, FunctionLiteral::kDeclaration, language_mode(),
                       CHECK_OK);
  return Statement::FunctionDeclaration();
}

PreParser::Statement PreParser::ParseAsyncFunctionDeclaration(bool* ok) {
  // AsyncFunctionDeclaration ::
  //   async [no LineTerminator here] function BindingIdentifier[Await]
  //       ( FormalParameters[Await] ) { AsyncFunctionBody }
  DCHECK_EQ(scanner()->current_token(), Token::ASYNC);
  int pos = position();
  Expect(Token::FUNCTION, CHECK_OK);
  ParseFunctionFlags flags = ParseFunctionFlags::kIsAsync;
  return ParseHoistableDeclaration(pos, flags, ok);
}

PreParser::Statement PreParser::ParseHoistableDeclaration(bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameterListopt ')' '{' FunctionBody '}'
  // GeneratorDeclaration ::
  //   'function' '*' Identifier '(' FormalParameterListopt ')'
  //      '{' FunctionBody '}'

  Expect(Token::FUNCTION, CHECK_OK);
  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlags::kIsNormal;
  if (Check(Token::MUL)) {
    flags |= ParseFunctionFlags::kIsGenerator;
  }
  return ParseHoistableDeclaration(pos, flags, ok);
}


PreParser::Statement PreParser::ParseClassDeclaration(bool* ok) {
  Expect(Token::CLASS, CHECK_OK);

  int pos = position();
  bool is_strict_reserved = false;
  Identifier name =
      ParseIdentifierOrStrictReservedWord(&is_strict_reserved, CHECK_OK);
  ParseClassLiteral(nullptr, name, scanner()->location(), is_strict_reserved,
                    pos, CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseBlock(bool* ok) {
  // Block ::
  //   '{' StatementList '}'

  Expect(Token::LBRACE, CHECK_OK);
  Statement final = Statement::Default();
  {
    BlockState block_state(&scope_state_);
    while (peek() != Token::RBRACE) {
      final = ParseStatementListItem(CHECK_OK);
    }
  }
  Expect(Token::RBRACE, ok);
  return final;
}


PreParser::Statement PreParser::ParseVariableStatement(
    VariableDeclarationContext var_context,
    bool* ok) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  Statement result = ParseVariableDeclarations(
      var_context, nullptr, nullptr, nullptr, nullptr, nullptr, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return result;
}


// If the variable declaration declares exactly one non-const
// variable, then *var is set to that variable. In all other cases,
// *var is untouched; in particular, it is the caller's responsibility
// to initialize it properly. This mechanism is also used for the parsing
// of 'for-in' loops.
PreParser::Statement PreParser::ParseVariableDeclarations(
    VariableDeclarationContext var_context, int* num_decl, bool* is_lexical,
    bool* is_binding_pattern, Scanner::Location* first_initializer_loc,
    Scanner::Location* bindings_loc, bool* ok) {
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
  bool lexical = false;
  bool is_pattern = false;
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
    DCHECK(var_context != kStatement);
    require_initializer = true;
    lexical = true;
  } else if (peek() == Token::LET) {
    Consume(Token::LET);
    DCHECK(var_context != kStatement);
    lexical = true;
  } else {
    *ok = false;
    return Statement::Default();
  }

  // The scope of a var/const declared variable anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). The scope
  // of a let declared variable is the scope of the immediately enclosing
  // block.
  int nvars = 0;  // the number of variables declared
  int bindings_start = peek_position();
  do {
    // Parse binding pattern.
    if (nvars > 0) Consume(Token::COMMA);
    int decl_pos = peek_position();
    PreParserExpression pattern = PreParserExpression::Default();
    {
      ExpressionClassifier pattern_classifier(this);
      pattern = ParsePrimaryExpression(&pattern_classifier, CHECK_OK);

      ValidateBindingPattern(&pattern_classifier, CHECK_OK);
      if (lexical) {
        ValidateLetPattern(&pattern_classifier, CHECK_OK);
      }
    }

    is_pattern = pattern.IsObjectLiteral() || pattern.IsArrayLiteral();

    Scanner::Location variable_loc = scanner()->location();
    nvars++;
    if (Check(Token::ASSIGN)) {
      ExpressionClassifier classifier(this);
      ParseAssignmentExpression(var_context != kForStatement, &classifier,
                                CHECK_OK);
      ValidateExpression(&classifier, CHECK_OK);

      variable_loc.end_pos = scanner()->location().end_pos;
      if (first_initializer_loc && !first_initializer_loc->IsValid()) {
        *first_initializer_loc = variable_loc;
      }
    } else if ((require_initializer || is_pattern) &&
               (var_context != kForStatement || !PeekInOrOf())) {
      ReportMessageAt(
          Scanner::Location(decl_pos, scanner()->location().end_pos),
          MessageTemplate::kDeclarationMissingInitializer,
          is_pattern ? "destructuring" : "const");
      *ok = false;
      return Statement::Default();
    }
  } while (peek() == Token::COMMA);

  if (bindings_loc) {
    *bindings_loc =
        Scanner::Location(bindings_start, scanner()->location().end_pos);
  }

  if (num_decl != nullptr) *num_decl = nvars;
  if (is_lexical != nullptr) *is_lexical = lexical;
  if (is_binding_pattern != nullptr) *is_binding_pattern = is_pattern;
  return Statement::Default();
}

PreParser::Statement PreParser::ParseFunctionDeclaration(bool* ok) {
  Consume(Token::FUNCTION);
  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlags::kIsNormal;
  if (Check(Token::MUL)) {
    flags |= ParseFunctionFlags::kIsGenerator;
    if (allow_harmony_restrictive_declarations()) {
      ReportMessageAt(scanner()->location(),
                      MessageTemplate::kGeneratorInLegacyContext);
      *ok = false;
      return Statement::Default();
    }
  }
  return ParseHoistableDeclaration(pos, flags, ok);
}

PreParser::Statement PreParser::ParseExpressionOrLabelledStatement(
    AllowLabelledFunctionStatement allow_function, bool* ok) {
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

    default:
      break;
  }

  bool starts_with_identifier = peek_any_identifier();
  ExpressionClassifier classifier(this);
  Expression expr = ParseExpression(true, &classifier, CHECK_OK);
  ValidateExpression(&classifier, CHECK_OK);

  // Even if the expression starts with an identifier, it is not necessarily an
  // identifier. For example, "foo + bar" starts with an identifier but is not
  // an identifier.
  if (starts_with_identifier && expr.IsIdentifier() && peek() == Token::COLON) {
    // Expression is a single identifier, and not, e.g., a parenthesized
    // identifier.
    DCHECK(!expr.AsIdentifier().IsEnum());
    DCHECK(!parsing_module_ || !expr.AsIdentifier().IsAwait());
    DCHECK(is_sloppy(language_mode()) ||
           !IsFutureStrictReserved(expr.AsIdentifier()));
    Consume(Token::COLON);
    // ES#sec-labelled-function-declarations Labelled Function Declarations
    if (peek() == Token::FUNCTION && is_sloppy(language_mode())) {
      if (allow_function == kAllowLabelledFunctionStatement) {
        return ParseFunctionDeclaration(ok);
      } else {
        return ParseScopedStatement(true, ok);
      }
    }
    Statement statement =
        ParseStatement(kDisallowLabelledFunctionStatement, ok);
    return statement.IsJumpStatement() ? Statement::Default() : statement;
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
  Statement stat = ParseScopedStatement(false, CHECK_OK);
  if (peek() == Token::ELSE) {
    Next();
    Statement else_stat = ParseScopedStatement(false, CHECK_OK);
    stat = (stat.IsJumpStatement() && else_stat.IsJumpStatement()) ?
        Statement::Jump() : Statement::Default();
  } else {
    stat = Statement::Default();
  }
  return stat;
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
    ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Jump();
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
    ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Jump();
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
    // Because of the return code rewriting that happens in case of a subclass
    // constructor we don't want to accept tail calls, therefore we don't set
    // ReturnExprScope to kInsideValidReturnStatement here.
    ReturnExprContext return_expr_context =
        IsSubclassConstructor(function_state_->kind())
            ? function_state_->return_expr_context()
            : ReturnExprContext::kInsideValidReturnStatement;

    ReturnExprScope maybe_allow_tail_calls(function_state_,
                                           return_expr_context);
    ParseExpression(true, CHECK_OK);
  }
  ExpectSemicolon(CHECK_OK);
  return Statement::Jump();
}


PreParser::Statement PreParser::ParseWithStatement(bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement
  Expect(Token::WITH, CHECK_OK);
  if (is_strict(language_mode())) {
    ReportMessageAt(scanner()->location(), MessageTemplate::kStrictWith);
    *ok = false;
    return Statement::Default();
  }
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  Scope* with_scope = NewScope(WITH_SCOPE);
  BlockState block_state(&scope_state_, with_scope);
  ParseScopedStatement(true, CHECK_OK);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseSwitchStatement(bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'

  Expect(Token::SWITCH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  {
    BlockState cases_block_state(&scope_state_);
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
      Statement statement = Statement::Jump();
      while (token != Token::CASE &&
             token != Token::DEFAULT &&
             token != Token::RBRACE) {
        statement = ParseStatementListItem(CHECK_OK);
        token = peek();
      }
    }
  }
  Expect(Token::RBRACE, ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseDoWhileStatement(bool* ok) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'

  Expect(Token::DO, CHECK_OK);
  ParseScopedStatement(true, CHECK_OK);
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
  ParseScopedStatement(true, ok);
  return Statement::Default();
}


PreParser::Statement PreParser::ParseForStatement(bool* ok) {
  // ForStatement ::
  //   'for' '(' Expression? ';' Expression? ';' Expression? ')' Statement

  // Create an in-between scope for let-bound iteration variables.
  bool has_lexical = false;

  BlockState block_state(&scope_state_);
  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  if (peek() != Token::SEMICOLON) {
    ForEachStatement::VisitMode mode;
    if (peek() == Token::VAR || peek() == Token::CONST ||
        (peek() == Token::LET && IsNextLetKeyword())) {
      int decl_count;
      bool is_lexical;
      bool is_binding_pattern;
      Scanner::Location first_initializer_loc = Scanner::Location::invalid();
      Scanner::Location bindings_loc = Scanner::Location::invalid();
      ParseVariableDeclarations(kForStatement, &decl_count, &is_lexical,
                                &is_binding_pattern, &first_initializer_loc,
                                &bindings_loc, CHECK_OK);
      if (is_lexical) has_lexical = true;
      if (CheckInOrOf(&mode, ok)) {
        if (!*ok) return Statement::Default();
        if (decl_count != 1) {
          ReportMessageAt(bindings_loc,
                          MessageTemplate::kForInOfLoopMultiBindings,
                          ForEachStatement::VisitModeString(mode));
          *ok = false;
          return Statement::Default();
        }
        if (first_initializer_loc.IsValid() &&
            (is_strict(language_mode()) || mode == ForEachStatement::ITERATE ||
             is_lexical || is_binding_pattern || allow_harmony_for_in())) {
          // Only increment the use count if we would have let this through
          // without the flag.
          if (use_counts_ != nullptr && allow_harmony_for_in()) {
            ++use_counts_[v8::Isolate::kForInInitializer];
          }
          ReportMessageAt(first_initializer_loc,
                          MessageTemplate::kForInOfLoopInitializer,
                          ForEachStatement::VisitModeString(mode));
          *ok = false;
          return Statement::Default();
        }

        if (mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          ParseAssignmentExpression(true, &classifier, CHECK_OK);
          RewriteNonPattern(&classifier, CHECK_OK);
        } else {
          ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);
        {
          ReturnExprScope no_tail_calls(function_state_,
                                        ReturnExprContext::kInsideForInOfBody);
          ParseScopedStatement(true, CHECK_OK);
        }
        return Statement::Default();
      }
    } else {
      int lhs_beg_pos = peek_position();
      ExpressionClassifier classifier(this);
      Expression lhs = ParseExpression(false, &classifier, CHECK_OK);
      int lhs_end_pos = scanner()->location().end_pos;
      bool is_for_each = CheckInOrOf(&mode, CHECK_OK);
      bool is_destructuring = is_for_each &&
                              (lhs->IsArrayLiteral() || lhs->IsObjectLiteral());

      if (is_destructuring) {
        ValidateAssignmentPattern(&classifier, CHECK_OK);
      } else {
        ValidateExpression(&classifier, CHECK_OK);
      }

      if (is_for_each) {
        if (!is_destructuring) {
          lhs = CheckAndRewriteReferenceExpression(
              lhs, lhs_beg_pos, lhs_end_pos, MessageTemplate::kInvalidLhsInFor,
              kSyntaxError, CHECK_OK);
        }

        if (mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          ParseAssignmentExpression(true, &classifier, CHECK_OK);
          RewriteNonPattern(&classifier, CHECK_OK);
        } else {
          ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);
        {
          BlockState block_state(&scope_state_);
          ParseScopedStatement(true, CHECK_OK);
        }
        return Statement::Default();
      }
    }
  }

  // Parsed initializer at this point.
  Expect(Token::SEMICOLON, CHECK_OK);

  // If there are let bindings, then condition and the next statement of the
  // for loop must be parsed in a new scope.
  Scope* inner_scope = scope();
  // TODO(verwaest): Allocate this through a ScopeState as well.
  if (has_lexical) inner_scope = NewScopeWithParent(inner_scope, BLOCK_SCOPE);

  {
    BlockState block_state(&scope_state_, inner_scope);

    if (peek() != Token::SEMICOLON) {
      ParseExpression(true, CHECK_OK);
    }
    Expect(Token::SEMICOLON, CHECK_OK);

    if (peek() != Token::RPAREN) {
      ParseExpression(true, CHECK_OK);
    }
    Expect(Token::RPAREN, CHECK_OK);

    ParseScopedStatement(true, ok);
  }
  return Statement::Default();
}


PreParser::Statement PreParser::ParseThrowStatement(bool* ok) {
  // ThrowStatement ::
  //   'throw' [no line terminator] Expression ';'

  Expect(Token::THROW, CHECK_OK);
  if (scanner()->HasAnyLineTerminatorBeforeNext()) {
    ReportMessageAt(scanner()->location(), MessageTemplate::kNewlineAfterThrow);
    *ok = false;
    return Statement::Default();
  }
  ParseExpression(true, CHECK_OK);
  ExpectSemicolon(ok);
  return Statement::Jump();
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

  {
    ReturnExprScope no_tail_calls(function_state_,
                                  ReturnExprContext::kInsideTryBlock);
    ParseBlock(CHECK_OK);
  }

  Token::Value tok = peek();
  if (tok != Token::CATCH && tok != Token::FINALLY) {
    ReportMessageAt(scanner()->location(), MessageTemplate::kNoCatchOrFinally);
    *ok = false;
    return Statement::Default();
  }
  TailCallExpressionList tail_call_expressions_in_catch_block(zone());
  bool catch_block_exists = false;
  if (tok == Token::CATCH) {
    Consume(Token::CATCH);
    Expect(Token::LPAREN, CHECK_OK);
    Scope* catch_scope = NewScope(CATCH_SCOPE);
    ExpressionClassifier pattern_classifier(this);
    ParsePrimaryExpression(&pattern_classifier, CHECK_OK);
    ValidateBindingPattern(&pattern_classifier, CHECK_OK);
    Expect(Token::RPAREN, CHECK_OK);
    {
      CollectExpressionsInTailPositionToListScope
          collect_tail_call_expressions_scope(
              function_state_, &tail_call_expressions_in_catch_block);
      BlockState block_state(&scope_state_, catch_scope);
      {
        BlockState block_state(&scope_state_);
        ParseBlock(CHECK_OK);
      }
    }
    catch_block_exists = true;
    tok = peek();
  }
  if (tok == Token::FINALLY) {
    Consume(Token::FINALLY);
    ParseBlock(CHECK_OK);
    if (FLAG_harmony_explicit_tailcalls && catch_block_exists &&
        tail_call_expressions_in_catch_block.has_explicit_tail_calls()) {
      // TODO(ishell): update chapter number.
      // ES8 XX.YY.ZZ
      ReportMessageAt(tail_call_expressions_in_catch_block.location(),
                      MessageTemplate::kUnexpectedTailCallInCatchBlock);
      *ok = false;
      return Statement::Default();
    }
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


// Redefinition of CHECK_OK for parsing expressions.
#undef CHECK_OK
#define CHECK_OK  ok);                     \
  if (!*ok) return Expression::Default();  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY


PreParser::Expression PreParser::ParseFunctionLiteral(
    Identifier function_name, Scanner::Location function_name_location,
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_pos, FunctionLiteral::FunctionType function_type,
    LanguageMode language_mode, bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  bool outer_is_script_scope = scope()->is_script_scope();
  DeclarationScope* function_scope = NewFunctionScope(kind);
  function_scope->SetLanguageMode(language_mode);
  FunctionState function_state(&function_state_, &scope_state_, function_scope,
                               kind);
  DuplicateFinder duplicate_finder(scanner()->unicode_cache());
  ExpressionClassifier formals_classifier(this, &duplicate_finder);

  Expect(Token::LPAREN, CHECK_OK);
  int start_position = scanner()->location().beg_pos;
  function_scope->set_start_position(start_position);
  PreParserFormalParameters formals(function_scope);
  ParseFormalParameterList(&formals, &formals_classifier, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  int formals_end_position = scanner()->location().end_pos;

  CheckArityRestrictions(formals.arity, kind, formals.has_rest, start_position,
                         formals_end_position, CHECK_OK);

  // See Parser::ParseFunctionLiteral for more information about lazy parsing
  // and lazy compilation.
  bool is_lazily_parsed = (outer_is_script_scope && allow_lazy() &&
                           !function_state_->this_function_is_parenthesized());

  Expect(Token::LBRACE, CHECK_OK);
  if (is_lazily_parsed) {
    ParseLazyFunctionLiteralBody(CHECK_OK);
  } else {
    ParseStatementList(Token::RBRACE, CHECK_OK);
  }
  Expect(Token::RBRACE, CHECK_OK);

  // Parsing the body may change the language mode in our scope.
  language_mode = function_scope->language_mode();

  // Validate name and parameter names. We can do this only after parsing the
  // function, since the function can declare itself strict.
  CheckFunctionName(language_mode, function_name, function_name_validity,
                    function_name_location, CHECK_OK);
  const bool allow_duplicate_parameters =
      is_sloppy(language_mode) && formals.is_simple && !IsConciseMethod(kind);
  ValidateFormalParameters(&formals_classifier, language_mode,
                           allow_duplicate_parameters, CHECK_OK);

  if (is_strict(language_mode)) {
    int end_position = scanner()->location().end_pos;
    CheckStrictOctalLiteral(start_position, end_position, CHECK_OK);
    CheckDecimalLiteralWithLeadingZero(use_counts_, start_position,
                                       end_position);
  }

  return Expression::Default();
}

PreParser::Expression PreParser::ParseAsyncFunctionExpression(bool* ok) {
  // AsyncFunctionDeclaration ::
  //   async [no LineTerminator here] function ( FormalParameters[Await] )
  //       { AsyncFunctionBody }
  //
  //   async [no LineTerminator here] function BindingIdentifier[Await]
  //       ( FormalParameters[Await] ) { AsyncFunctionBody }
  int pos = position();
  Expect(Token::FUNCTION, CHECK_OK);
  bool is_strict_reserved = false;
  Identifier name;
  FunctionLiteral::FunctionType type = FunctionLiteral::kAnonymousExpression;

  if (peek_any_identifier()) {
    type = FunctionLiteral::kNamedExpression;
    name = ParseIdentifierOrStrictReservedWord(FunctionKind::kAsyncFunction,
                                               &is_strict_reserved, CHECK_OK);
  }

  ParseFunctionLiteral(name, scanner()->location(),
                       is_strict_reserved ? kFunctionNameIsStrictReserved
                                          : kFunctionNameValidityUnknown,
                       FunctionKind::kAsyncFunction, pos, type, language_mode(),
                       CHECK_OK);
  return Expression::Default();
}

void PreParser::ParseLazyFunctionLiteralBody(bool* ok,
                                             Scanner::BookmarkScope* bookmark) {
  int body_start = position();
  ParseStatementList(Token::RBRACE, ok, bookmark);
  if (!*ok) return;
  if (bookmark && bookmark->HasBeenReset()) return;

  // Position right after terminal '}'.
  DCHECK_EQ(Token::RBRACE, scanner()->peek());
  int body_end = scanner()->peek_location().end_pos;
  DeclarationScope* scope = this->scope()->AsDeclarationScope();
  DCHECK(scope->is_function_scope());
  log_->LogFunction(body_start, body_end,
                    function_state_->materialized_literal_count(),
                    function_state_->expected_property_count(), language_mode(),
                    scope->uses_super_property(), scope->calls_eval());
}

PreParserExpression PreParser::ParseClassLiteral(
    ExpressionClassifier* classifier, PreParserIdentifier name,
    Scanner::Location class_name_location, bool name_is_strict_reserved,
    int pos, bool* ok) {
  // All parts of a ClassDeclaration and ClassExpression are strict code.
  if (name_is_strict_reserved) {
    ReportMessageAt(class_name_location,
                    MessageTemplate::kUnexpectedStrictReserved);
    *ok = false;
    return EmptyExpression();
  }
  if (IsEvalOrArguments(name)) {
    ReportMessageAt(class_name_location, MessageTemplate::kStrictEvalArguments);
    *ok = false;
    return EmptyExpression();
  }

  LanguageMode class_language_mode = language_mode();
  BlockState block_state(&scope_state_);
  scope()->SetLanguageMode(
      static_cast<LanguageMode>(class_language_mode | STRICT));
  // TODO(marja): Make PreParser use scope names too.
  // this->scope()->SetScopeName(name);

  bool has_extends = Check(Token::EXTENDS);
  if (has_extends) {
    ExpressionClassifier extends_classifier(this);
    ParseLeftHandSideExpression(&extends_classifier, CHECK_OK);
    CheckNoTailCallExpressions(&extends_classifier, CHECK_OK);
    ValidateExpression(&extends_classifier, CHECK_OK);
    if (classifier != nullptr) {
      classifier->Accumulate(&extends_classifier,
                             ExpressionClassifier::ExpressionProductions);
    }
  }

  ClassLiteralChecker checker(this);
  bool has_seen_constructor = false;

  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    if (Check(Token::SEMICOLON)) continue;
    const bool in_class = true;
    bool is_computed_name = false;  // Classes do not care about computed
                                    // property names here.
    Identifier name;
    ExpressionClassifier property_classifier(this);
    ParsePropertyDefinition(
        &checker, in_class, has_extends, MethodKind::kNormal, &is_computed_name,
        &has_seen_constructor, &property_classifier, &name, CHECK_OK);
    ValidateExpression(&property_classifier, CHECK_OK);
    if (classifier != nullptr) {
      classifier->Accumulate(&property_classifier,
                             ExpressionClassifier::ExpressionProductions);
    }
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
  ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  Scanner::Location spread_pos;
  ExpressionClassifier classifier(this);
  ParseArguments(&spread_pos, &classifier, ok);
  ValidateExpression(&classifier, CHECK_OK);

  DCHECK(!spread_pos.IsValid());

  return Expression::Default();
}


PreParserExpression PreParser::ParseDoExpression(bool* ok) {
  // AssignmentExpression ::
  //     do '{' StatementList '}'
  Expect(Token::DO, CHECK_OK);
  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    ParseStatementListItem(CHECK_OK);
  }
  Expect(Token::RBRACE, CHECK_OK);
  return PreParserExpression::Default();
}

void PreParser::ParseAsyncArrowSingleExpressionBody(
    PreParserStatementList body, bool accept_IN,
    ExpressionClassifier* classifier, int pos, bool* ok) {
  scope()->ForceContextAllocation();

  PreParserExpression return_value =
      ParseAssignmentExpression(accept_IN, classifier, CHECK_OK_CUSTOM(Void));

  body->Add(PreParserStatement::ExpressionStatement(return_value), zone());
}

#undef CHECK_OK
#undef CHECK_OK_CUSTOM


}  // namespace internal
}  // namespace v8
