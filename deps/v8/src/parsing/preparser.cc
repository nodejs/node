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
#include "src/parsing/duplicate-finder.h"
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

#define CHECK_OK_VALUE(x) ok); \
  if (!*ok) return x;          \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

#define CHECK_OK CHECK_OK_VALUE(Expression::Default())
#define CHECK_OK_VOID CHECK_OK_VALUE(this->Void())

namespace {

PreParserIdentifier GetSymbolHelper(Scanner* scanner) {
  switch (scanner->current_token()) {
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
      if (scanner->UnescapedLiteralMatches("eval", 4))
        return PreParserIdentifier::Eval();
      if (scanner->UnescapedLiteralMatches("arguments", 9))
        return PreParserIdentifier::Arguments();
      if (scanner->UnescapedLiteralMatches("undefined", 9))
        return PreParserIdentifier::Undefined();
      if (scanner->LiteralMatches("prototype", 9))
        return PreParserIdentifier::Prototype();
      if (scanner->LiteralMatches("constructor", 11))
        return PreParserIdentifier::Constructor();
      return PreParserIdentifier::Default();
  }
}

}  // unnamed namespace

PreParserIdentifier PreParser::GetSymbol() const {
  PreParserIdentifier symbol = GetSymbolHelper(scanner());
  if (track_unresolved_variables_) {
    const AstRawString* result = scanner()->CurrentSymbol(ast_value_factory());
    DCHECK_NOT_NULL(result);
    symbol.string_ = result;
  }
  return symbol;
}

PreParser::PreParseResult PreParser::PreParseLazyFunction(
    DeclarationScope* function_scope, bool parsing_module, ParserRecorder* log,
    bool is_inner_function, bool may_abort, int* use_counts) {
  DCHECK_EQ(FUNCTION_SCOPE, function_scope->scope_type());
  parsing_module_ = parsing_module;
  log_ = log;
  use_counts_ = use_counts;
  DCHECK(!track_unresolved_variables_);
  track_unresolved_variables_ = is_inner_function;

  // The caller passes the function_scope which is not yet inserted into the
  // scope_state_. All scopes above the function_scope are ignored by the
  // PreParser.
  DCHECK_NULL(scope_state_);
  FunctionState function_state(&function_state_, &scope_state_, function_scope);
  DCHECK_EQ(Token::LBRACE, scanner()->current_token());
  bool ok = true;
  int start_position = peek_position();
  LazyParsingResult result = ParseLazyFunctionLiteralBody(may_abort, &ok);
  use_counts_ = nullptr;
  track_unresolved_variables_ = false;
  if (result == kLazyParsingAborted) {
    return kPreParseAbort;
  } else if (stack_overflow()) {
    return kPreParseStackOverflow;
  } else if (!ok) {
    ReportUnexpectedToken(scanner()->current_token());
  } else {
    DCHECK_EQ(Token::RBRACE, scanner()->peek());
    if (is_strict(function_scope->language_mode())) {
      int end_pos = scanner()->location().end_pos;
      CheckStrictOctalLiteral(start_position, end_pos, &ok);
      CheckDecimalLiteralWithLeadingZero(start_position, end_pos);
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

PreParser::Expression PreParser::ParseFunctionLiteral(
    Identifier function_name, Scanner::Location function_name_location,
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_pos, FunctionLiteral::FunctionType function_type,
    LanguageMode language_mode, bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'

  // Parse function body.
  PreParserStatementList body;
  bool outer_is_script_scope = scope()->is_script_scope();
  DeclarationScope* function_scope = NewFunctionScope(kind);
  function_scope->SetLanguageMode(language_mode);
  FunctionState function_state(&function_state_, &scope_state_, function_scope);
  DuplicateFinder duplicate_finder(scanner()->unicode_cache());
  ExpressionClassifier formals_classifier(this, &duplicate_finder);

  Expect(Token::LPAREN, CHECK_OK);
  int start_position = scanner()->location().beg_pos;
  function_scope->set_start_position(start_position);
  PreParserFormalParameters formals(function_scope);
  ParseFormalParameterList(&formals, CHECK_OK);
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
    ParseLazyFunctionLiteralBody(false, CHECK_OK);
  } else {
    ParseStatementList(body, Token::RBRACE, CHECK_OK);
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
  ValidateFormalParameters(language_mode, allow_duplicate_parameters, CHECK_OK);

  if (is_strict(language_mode)) {
    int end_position = scanner()->location().end_pos;
    CheckStrictOctalLiteral(start_position, end_position, CHECK_OK);
    CheckDecimalLiteralWithLeadingZero(start_position, end_position);
  }

  return Expression::Default();
}

PreParser::LazyParsingResult PreParser::ParseLazyFunctionLiteralBody(
    bool may_abort, bool* ok) {
  int body_start = position();
  PreParserStatementList body;
  LazyParsingResult result = ParseStatementList(
      body, Token::RBRACE, may_abort, CHECK_OK_VALUE(kLazyParsingComplete));
  if (result == kLazyParsingAborted) return result;

  // Position right after terminal '}'.
  DCHECK_EQ(Token::RBRACE, scanner()->peek());
  int body_end = scanner()->peek_location().end_pos;
  DeclarationScope* scope = this->scope()->AsDeclarationScope();
  DCHECK(scope->is_function_scope());
  log_->LogFunction(body_start, body_end,
                    function_state_->materialized_literal_count(),
                    function_state_->expected_property_count(), language_mode(),
                    scope->uses_super_property(), scope->calls_eval());
  return kLazyParsingComplete;
}

PreParserExpression PreParser::ExpressionFromIdentifier(
    PreParserIdentifier name, int start_position, int end_position,
    InferName infer) {
  if (track_unresolved_variables_) {
    AstNodeFactory factory(ast_value_factory());
    // Setting the Zone is necessary because zone_ might be the temp Zone, and
    // AstValueFactory doesn't know about it.
    factory.set_zone(zone());
    DCHECK_NOT_NULL(name.string_);
    scope()->NewUnresolved(&factory, name.string_, start_position, end_position,
                           NORMAL_VARIABLE);
  }
  return PreParserExpression::FromIdentifier(name);
}

void PreParser::DeclareAndInitializeVariables(
    PreParserStatement block,
    const DeclarationDescriptor* declaration_descriptor,
    const DeclarationParsingResult::Declaration* declaration,
    ZoneList<const AstRawString*>* names, bool* ok) {
  if (declaration->pattern.string_) {
    /* Mimic what Parser does when declaring variables (see
       Parser::PatternRewriter::VisitVariableProxy).

       var + no initializer -> RemoveUnresolved
       let + no initializer -> RemoveUnresolved
       var + initializer -> RemoveUnresolved followed by NewUnresolved
       let + initializer -> RemoveUnresolved
    */

    if (declaration->initializer.IsEmpty() ||
        declaration_descriptor->mode == VariableMode::LET) {
      declaration_descriptor->scope->RemoveUnresolved(
          declaration->pattern.string_);
    }
  }
}

#undef CHECK_OK
#undef CHECK_OK_CUSTOM


}  // namespace internal
}  // namespace v8
