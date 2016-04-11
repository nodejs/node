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
#include "src/parsing/parser-base.h"
#include "src/parsing/preparse-data.h"
#include "src/parsing/preparse-data-format.h"
#include "src/parsing/preparser.h"
#include "src/unicode.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

void PreParserTraits::ReportMessageAt(Scanner::Location location,
                                      MessageTemplate::Template message,
                                      const char* arg,
                                      ParseErrorType error_type) {
  ReportMessageAt(location.beg_pos, location.end_pos, message, arg, error_type);
}


void PreParserTraits::ReportMessageAt(int start_pos, int end_pos,
                                      MessageTemplate::Template message,
                                      const char* arg,
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
  if (scanner->UnescapedLiteralMatches("undefined", 9)) {
    return PreParserIdentifier::Undefined();
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
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_position, FunctionLiteral::FunctionType type,
    FunctionLiteral::ArityRestriction arity_restriction,
    LanguageMode language_mode, bool* ok) {
  return pre_parser_->ParseFunctionLiteral(
      name, function_name_location, function_name_validity, kind,
      function_token_position, type, arity_restriction, language_mode, ok);
}


PreParser::PreParseResult PreParser::PreParseLazyFunction(
    LanguageMode language_mode, FunctionKind kind, bool has_simple_parameters,
    ParserRecorder* log, Scanner::BookmarkScope* bookmark) {
  log_ = log;
  // Lazy functions always have trivial outer scopes (no with/catch scopes).
  Scope* top_scope = NewScope(scope_, SCRIPT_SCOPE);
  PreParserFactory top_factory(NULL);
  FunctionState top_state(&function_state_, &scope_, top_scope, kNormalFunction,
                          &top_factory);
  scope_->SetLanguageMode(language_mode);
  Scope* function_scope = NewScope(scope_, FUNCTION_SCOPE, kind);
  if (!has_simple_parameters) function_scope->SetHasNonSimpleParameters();
  PreParserFactory function_factory(NULL);
  FunctionState function_state(&function_state_, &scope_, function_scope, kind,
                               &function_factory);
  DCHECK_EQ(Token::LBRACE, scanner()->current_token());
  bool ok = true;
  int start_position = peek_position();
  ParseLazyFunctionLiteralBody(&ok, bookmark);
  if (bookmark && bookmark->HasBeenReset()) {
    // Do nothing, as we've just aborted scanning this function.
  } else if (stack_overflow()) {
    return kPreParseStackOverflow;
  } else if (!ok) {
    ReportUnexpectedToken(scanner()->current_token());
  } else {
    DCHECK_EQ(Token::RBRACE, scanner()->peek());
    if (is_strict(scope_->language_mode())) {
      int end_pos = scanner()->location().end_pos;
      CheckStrictOctalLiteral(start_position, end_pos, &ok);
      if (!ok) return kPreParseSuccess;

      if (is_strong(scope_->language_mode()) && IsSubclassConstructor(kind)) {
        if (!function_state.super_location().IsValid()) {
          ReportMessageAt(Scanner::Location(start_position, start_position + 1),
                          MessageTemplate::kStrongSuperCallMissing,
                          kReferenceError);
          return kPreParseSuccess;
        }
      }
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
      if (allow_const()) {
        return ParseVariableStatement(kStatementListItem, ok);
      }
      break;
    case Token::LET:
      if (IsNextLetKeyword()) {
        return ParseVariableStatement(kStatementListItem, ok);
      }
      break;
    default:
      break;
  }
  return ParseStatement(ok);
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
    Scanner::Location old_this_loc = function_state_->this_location();
    Scanner::Location old_super_loc = function_state_->super_location();
    Statement statement = ParseStatementListItem(ok);
    if (!*ok) return;

    if (is_strong(language_mode()) && scope_->is_function_scope() &&
        IsClassConstructor(function_state_->kind())) {
      Scanner::Location this_loc = function_state_->this_location();
      Scanner::Location super_loc = function_state_->super_location();
      if (this_loc.beg_pos != old_this_loc.beg_pos &&
          this_loc.beg_pos != token_loc.beg_pos) {
        ReportMessageAt(this_loc, MessageTemplate::kStrongConstructorThis);
        *ok = false;
        return;
      }
      if (super_loc.beg_pos != old_super_loc.beg_pos &&
          super_loc.beg_pos != token_loc.beg_pos) {
        ReportMessageAt(super_loc, MessageTemplate::kStrongConstructorSuper);
        *ok = false;
        return;
      }
    }

    if (directive_prologue) {
      bool use_strict_found = statement.IsUseStrictLiteral();
      bool use_strong_found =
          statement.IsUseStrongLiteral() && allow_strong_mode();

      if (use_strict_found) {
        scope_->SetLanguageMode(
            static_cast<LanguageMode>(scope_->language_mode() | STRICT));
      } else if (use_strong_found) {
        scope_->SetLanguageMode(static_cast<LanguageMode>(
            scope_->language_mode() | STRONG));
        if (IsClassConstructor(function_state_->kind())) {
          // "use strong" cannot occur in a class constructor body, to avoid
          // unintuitive strong class object semantics.
          PreParserTraits::ReportMessageAt(
              token_loc, MessageTemplate::kStrongConstructorDirective);
          *ok = false;
          return;
        }
      } else if (!statement.IsStringLiteral()) {
        directive_prologue = false;
      }

      if ((use_strict_found || use_strong_found) &&
          !scope_->HasSimpleParameters()) {
        // TC39 deemed "use strict" directives to be an error when occurring
        // in the body of a function with non-simple parameter list, on
        // 29/7/2015. https://goo.gl/ueA7Ln
        //
        // In V8, this also applies to "use strong " directives.
        PreParserTraits::ReportMessageAt(
            token_loc, MessageTemplate::kIllegalLanguageModeDirective,
            use_strict_found ? "use strict" : "use strong");
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


#define CHECK_OK  ok);                   \
  if (!*ok) return Statement::Default();  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY


PreParser::Statement PreParser::ParseStatement(bool* ok) {
  // Statement ::
  //   EmptyStatement
  //   ...

  if (peek() == Token::SEMICOLON) {
    Next();
    return Statement::Default();
  }
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
                                         MessageTemplate::kStrongEmpty);
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
                                         MessageTemplate::kStrictFunction);
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
      if (is_sloppy(language_mode()) && allow_legacy_const()) {
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
  ParseFunctionLiteral(name, scanner()->location(),
                       is_strict_reserved ? kFunctionNameIsStrictReserved
                                          : kFunctionNameValidityUnknown,
                       is_generator ? FunctionKind::kGeneratorFunction
                                    : FunctionKind::kNormalFunction,
                       pos, FunctionLiteral::kDeclaration,
                       FunctionLiteral::kNormalArity, language_mode(),
                       CHECK_OK);
  return Statement::FunctionDeclaration();
}


PreParser::Statement PreParser::ParseClassDeclaration(bool* ok) {
  Expect(Token::CLASS, CHECK_OK);
  if (!allow_harmony_sloppy() && is_sloppy(language_mode())) {
    ReportMessage(MessageTemplate::kSloppyLexical);
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
  //   '{' StatementList '}'

  Expect(Token::LBRACE, CHECK_OK);
  Statement final = Statement::Default();
  while (peek() != Token::RBRACE) {
    final = ParseStatementListItem(CHECK_OK);
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
    if (is_strong(language_mode())) {
      Scanner::Location location = scanner()->peek_location();
      ReportMessageAt(location, MessageTemplate::kStrongVar);
      *ok = false;
      return Statement::Default();
    }
    Consume(Token::VAR);
  } else if (peek() == Token::CONST && allow_const()) {
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
    if (is_strict(language_mode()) ||
        (allow_harmony_sloppy() && !allow_legacy_const())) {
      DCHECK(var_context != kStatement);
      require_initializer = true;
      lexical = true;
    }
  } else if (peek() == Token::LET && allow_let()) {
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
      ExpressionClassifier pattern_classifier;
      Token::Value next = peek();
      pattern = ParsePrimaryExpression(&pattern_classifier, CHECK_OK);

      ValidateBindingPattern(&pattern_classifier, CHECK_OK);
      if (lexical) {
        ValidateLetPattern(&pattern_classifier, CHECK_OK);
      }

      if (!allow_harmony_destructuring_bind() && !pattern.IsIdentifier()) {
        ReportUnexpectedToken(next);
        *ok = false;
        return Statement::Default();
      }
    }

    is_pattern = (pattern.IsObjectLiteral() || pattern.IsArrayLiteral()) &&
                 !pattern.is_parenthesized();

    bool is_for_iteration_variable =
        var_context == kForStatement &&
        (peek() == Token::IN || PeekContextualKeyword(CStrVector("of")));

    Scanner::Location variable_loc = scanner()->location();
    nvars++;
    if (Check(Token::ASSIGN)) {
      ExpressionClassifier classifier;
      ParseAssignmentExpression(var_context != kForStatement, &classifier,
                                CHECK_OK);
      ValidateExpression(&classifier, CHECK_OK);

      variable_loc.end_pos = scanner()->location().end_pos;
      if (first_initializer_loc && !first_initializer_loc->IsValid()) {
        *first_initializer_loc = variable_loc;
      }
    } else if ((require_initializer || is_pattern) &&
               !is_for_iteration_variable) {
      PreParserTraits::ReportMessageAt(
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

    case Token::THIS:
      if (!FLAG_strong_this) break;
      // Fall through.
    case Token::SUPER:
      if (is_strong(language_mode()) &&
          IsClassConstructor(function_state_->kind())) {
        bool is_this = peek() == Token::THIS;
        Expression expr = Expression::Default();
        ExpressionClassifier classifier;
        if (is_this) {
          expr = ParseStrongInitializationExpression(&classifier, CHECK_OK);
        } else {
          expr = ParseStrongSuperCallExpression(&classifier, CHECK_OK);
        }
        ValidateExpression(&classifier, CHECK_OK);
        switch (peek()) {
          case Token::SEMICOLON:
            Consume(Token::SEMICOLON);
            break;
          case Token::RBRACE:
          case Token::EOS:
            break;
          default:
            if (!scanner()->HasAnyLineTerminatorBeforeNext()) {
              ReportMessageAt(function_state_->this_location(),
                              is_this
                                  ? MessageTemplate::kStrongConstructorThis
                                  : MessageTemplate::kStrongConstructorSuper);
              *ok = false;
              return Statement::Default();
            }
        }
        return Statement::ExpressionStatement(expr);
      }
      break;

    // TODO(arv): Handle `let [`
    // https://code.google.com/p/v8/issues/detail?id=3847

    default:
      break;
  }

  bool starts_with_identifier = peek_any_identifier();
  ExpressionClassifier classifier;
  Expression expr = ParseExpression(true, &classifier, CHECK_OK);
  ValidateExpression(&classifier, CHECK_OK);

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
    Statement statement = ParseStatement(ok);
    return statement.IsJumpStatement() ? Statement::Default() : statement;
    // Preparsing is disabled for extensions (because the extension details
    // aren't passed to lazily compiled functions), so we don't
    // accept "native function" in the preparser.
  }
  // Parsed expression statement.
  // Detect attempts at 'let' declarations in sloppy mode.
  if (!allow_harmony_sloppy_let() && peek() == Token::IDENTIFIER &&
      is_sloppy(language_mode()) && expr.IsIdentifier() &&
      expr.AsIdentifier().IsLet()) {
    ReportMessage(MessageTemplate::kSloppyLexical, NULL);
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
  Statement stat = ParseSubStatement(CHECK_OK);
  if (peek() == Token::ELSE) {
    Next();
    Statement else_stat = ParseSubStatement(CHECK_OK);
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
        IsClassConstructor(function_state_->kind())) {
      int pos = peek_position();
      ReportMessageAt(Scanner::Location(pos, pos + 1),
                      MessageTemplate::kStrongConstructorReturnValue);
      *ok = false;
      return Statement::Default();
    }
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
    Statement statement = Statement::Jump();
    while (token != Token::CASE &&
           token != Token::DEFAULT &&
           token != Token::RBRACE) {
      statement = ParseStatementListItem(CHECK_OK);
      token = peek();
    }
    if (is_strong(language_mode()) && !statement.IsJumpStatement() &&
        token != Token::RBRACE) {
      ReportMessageAt(scanner()->location(),
                      MessageTemplate::kStrongSwitchFallthrough);
      *ok = false;
      return Statement::Default();
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
    ForEachStatement::VisitMode mode;
    if (peek() == Token::VAR || (peek() == Token::CONST && allow_const()) ||
        (peek() == Token::LET && IsNextLetKeyword())) {
      int decl_count;
      bool is_lexical;
      bool is_binding_pattern;
      Scanner::Location first_initializer_loc = Scanner::Location::invalid();
      Scanner::Location bindings_loc = Scanner::Location::invalid();
      ParseVariableDeclarations(kForStatement, &decl_count, &is_lexical,
                                &is_binding_pattern, &first_initializer_loc,
                                &bindings_loc, CHECK_OK);
      bool accept_IN = decl_count >= 1;
      if (accept_IN && CheckInOrOf(&mode, ok)) {
        if (!*ok) return Statement::Default();
        if (decl_count != 1) {
          const char* loop_type =
              mode == ForEachStatement::ITERATE ? "for-of" : "for-in";
          PreParserTraits::ReportMessageAt(
              bindings_loc, MessageTemplate::kForInOfLoopMultiBindings,
              loop_type);
          *ok = false;
          return Statement::Default();
        }
        if (first_initializer_loc.IsValid() &&
            (is_strict(language_mode()) || mode == ForEachStatement::ITERATE ||
             is_lexical || is_binding_pattern)) {
          if (mode == ForEachStatement::ITERATE) {
            ReportMessageAt(first_initializer_loc,
                            MessageTemplate::kForOfLoopInitializer);
          } else {
            // TODO(caitp): This should be an error in sloppy mode, too.
            ReportMessageAt(first_initializer_loc,
                            MessageTemplate::kForInLoopInitializer);
          }
          *ok = false;
          return Statement::Default();
        }
        ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);
        ParseSubStatement(CHECK_OK);
        return Statement::Default();
      }
    } else {
      int lhs_beg_pos = peek_position();
      ExpressionClassifier classifier;
      Expression lhs = ParseExpression(false, &classifier, CHECK_OK);
      int lhs_end_pos = scanner()->location().end_pos;
      is_let_identifier_expression =
          lhs.IsIdentifier() && lhs.AsIdentifier().IsLet();
      bool is_for_each = CheckInOrOf(&mode, ok);
      if (!*ok) return Statement::Default();
      bool is_destructuring = is_for_each &&
                              allow_harmony_destructuring_assignment() &&
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
        ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);
        ParseSubStatement(CHECK_OK);
        return Statement::Default();
      }
    }
  }

  // Parsed initializer at this point.
  // Detect attempts at 'let' declarations in sloppy mode.
  if (!allow_harmony_sloppy_let() && peek() == Token::IDENTIFIER &&
      is_sloppy(language_mode()) && is_let_identifier_expression) {
    ReportMessage(MessageTemplate::kSloppyLexical, NULL);
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

  ParseBlock(CHECK_OK);

  Token::Value tok = peek();
  if (tok != Token::CATCH && tok != Token::FINALLY) {
    ReportMessageAt(scanner()->location(), MessageTemplate::kNoCatchOrFinally);
    *ok = false;
    return Statement::Default();
  }
  if (tok == Token::CATCH) {
    Consume(Token::CATCH);
    Expect(Token::LPAREN, CHECK_OK);
    ExpressionClassifier pattern_classifier;
    ParsePrimaryExpression(&pattern_classifier, CHECK_OK);
    ValidateBindingPattern(&pattern_classifier, CHECK_OK);
    Expect(Token::RPAREN, CHECK_OK);
    {
      // TODO(adamk): Make this CATCH_SCOPE
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
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_pos, FunctionLiteral::FunctionType function_type,
    FunctionLiteral::ArityRestriction arity_restriction,
    LanguageMode language_mode, bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  bool outer_is_script_scope = scope_->is_script_scope();
  Scope* function_scope = NewScope(scope_, FUNCTION_SCOPE, kind);
  function_scope->SetLanguageMode(language_mode);
  PreParserFactory factory(NULL);
  FunctionState function_state(&function_state_, &scope_, function_scope, kind,
                               &factory);
  DuplicateFinder duplicate_finder(scanner()->unicode_cache());
  ExpressionClassifier formals_classifier(&duplicate_finder);

  Expect(Token::LPAREN, CHECK_OK);
  int start_position = scanner()->location().beg_pos;
  function_scope->set_start_position(start_position);
  PreParserFormalParameters formals(function_scope);
  ParseFormalParameterList(&formals, &formals_classifier, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  int formals_end_position = scanner()->location().end_pos;

  CheckArityRestrictions(formals.arity, arity_restriction,
                         formals.has_rest, start_position,
                         formals_end_position, CHECK_OK);

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
  }

  if (is_strong(language_mode) && IsSubclassConstructor(kind)) {
    if (!function_state.super_location().IsValid()) {
      ReportMessageAt(function_name_location,
                      MessageTemplate::kStrongSuperCallMissing,
                      kReferenceError);
      *ok = false;
      return Expression::Default();
    }
  }

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
  log_->LogFunction(body_start, body_end,
                    function_state_->materialized_literal_count(),
                    function_state_->expected_property_count(), language_mode(),
                    scope_->uses_super_property(), scope_->calls_eval());
}


PreParserExpression PreParser::ParseClassLiteral(
    PreParserIdentifier name, Scanner::Location class_name_location,
    bool name_is_strict_reserved, int pos, bool* ok) {
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
  if (is_strong(class_language_mode) && IsUndefined(name)) {
    ReportMessageAt(class_name_location, MessageTemplate::kStrongUndefined);
    *ok = false;
    return EmptyExpression();
  }

  Scope* scope = NewScope(scope_, BLOCK_SCOPE);
  BlockState block_state(&scope_, scope);
  scope_->SetLanguageMode(
      static_cast<LanguageMode>(class_language_mode | STRICT));
  // TODO(marja): Make PreParser use scope names too.
  // scope_->SetScopeName(name);

  bool has_extends = Check(Token::EXTENDS);
  if (has_extends) {
    ExpressionClassifier classifier;
    ParseLeftHandSideExpression(&classifier, CHECK_OK);
    ValidateExpression(&classifier, CHECK_OK);
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
    Identifier name;
    ExpressionClassifier classifier;
    ParsePropertyDefinition(&checker, in_class, has_extends, is_static,
                            &is_computed_name, &has_seen_constructor,
                            &classifier, &name, CHECK_OK);
    ValidateExpression(&classifier, CHECK_OK);
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
  ExpressionClassifier classifier;
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
  Scope* block_scope = NewScope(scope_, BLOCK_SCOPE);
  {
    BlockState block_state(&scope_, block_scope);
    while (peek() != Token::RBRACE) {
      ParseStatementListItem(CHECK_OK);
    }
    Expect(Token::RBRACE, CHECK_OK);
    return PreParserExpression::Default();
  }
}

#undef CHECK_OK


}  // namespace internal
}  // namespace v8
