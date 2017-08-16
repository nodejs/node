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
#include "src/parsing/preparsed-scope-data.h"
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
  // These symbols require slightly different treatement:
  // - regular keywords (enum, await, etc.; treated in 1st switch.)
  // - 'contextual' keywords (and may contain escaped; treated in 2nd switch.)
  // - 'contextual' keywords, but may not be escaped (3rd switch).
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
      break;
  }
  switch (scanner->current_contextual_token()) {
    case Token::PROTOTYPE:
      return PreParserIdentifier::Prototype();
    case Token::CONSTRUCTOR:
      return PreParserIdentifier::Constructor();
    case Token::NAME:
      return PreParserIdentifier::Name();
    default:
      break;
  }
  if (scanner->literal_contains_escapes())
    return PreParserIdentifier::Default();
  switch (scanner->current_contextual_token()) {
    case Token::EVAL:
      return PreParserIdentifier::Eval();
    case Token::ARGUMENTS:
      return PreParserIdentifier::Arguments();
    case Token::UNDEFINED:
      return PreParserIdentifier::Undefined();
    default:
      break;
  }
  return PreParserIdentifier::Default();
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

PreParser::PreParseResult PreParser::PreParseProgram(bool is_module) {
  DCHECK_NULL(scope_);
  DeclarationScope* scope = NewScriptScope();
#ifdef DEBUG
  scope->set_is_being_lazily_parsed(true);
#endif

  // ModuleDeclarationInstantiation for Source Text Module Records creates a
  // new Module Environment Record whose outer lexical environment record is
  // the global scope.
  if (is_module) scope = NewModuleScope(scope);

  FunctionState top_scope(&function_state_, &scope_, scope);
  original_scope_ = scope_;
  bool ok = true;
  int start_position = scanner()->peek_location().beg_pos;
  parsing_module_ = is_module;
  PreParserStatementList body;
  ParseStatementList(body, Token::EOS, &ok);
  original_scope_ = nullptr;
  if (stack_overflow()) return kPreParseStackOverflow;
  if (!ok) {
    ReportUnexpectedToken(scanner()->current_token());
  } else if (is_strict(language_mode())) {
    CheckStrictOctalLiteral(start_position, scanner()->location().end_pos, &ok);
  }
  return kPreParseSuccess;
}

PreParser::PreParseResult PreParser::PreParseFunction(
    FunctionKind kind, DeclarationScope* function_scope, bool parsing_module,
    bool is_inner_function, bool may_abort, int* use_counts) {
  DCHECK_EQ(FUNCTION_SCOPE, function_scope->scope_type());
  parsing_module_ = parsing_module;
  use_counts_ = use_counts;
  DCHECK(!track_unresolved_variables_);
  track_unresolved_variables_ = is_inner_function;
#ifdef DEBUG
  function_scope->set_is_being_lazily_parsed(true);
#endif

  // In the preparser, we use the function literal ids to count how many
  // FunctionLiterals were encountered. The PreParser doesn't actually persist
  // FunctionLiterals, so there IDs don't matter.
  ResetFunctionLiteralId();

  // The caller passes the function_scope which is not yet inserted into the
  // scope stack. All scopes above the function_scope are ignored by the
  // PreParser.
  DCHECK_NULL(function_state_);
  DCHECK_NULL(scope_);
  FunctionState function_state(&function_state_, &scope_, function_scope);
  // This indirection is needed so that we can use the CHECK_OK macros.
  bool ok_holder = true;
  bool* ok = &ok_holder;

  PreParserFormalParameters formals(function_scope);
  DuplicateFinder duplicate_finder;
  std::unique_ptr<ExpressionClassifier> formals_classifier;

  // Parse non-arrow function parameters. For arrow functions, the parameters
  // have already been parsed.
  if (!IsArrowFunction(kind)) {
    formals_classifier.reset(new ExpressionClassifier(this, &duplicate_finder));
    // We return kPreParseSuccess in failure cases too - errors are retrieved
    // separately by Parser::SkipLazyFunctionBody.
    ParseFormalParameterList(&formals, CHECK_OK_VALUE(kPreParseSuccess));
    Expect(Token::RPAREN, CHECK_OK_VALUE(kPreParseSuccess));
    int formals_end_position = scanner()->location().end_pos;

    CheckArityRestrictions(
        formals.arity, kind, formals.has_rest, function_scope->start_position(),
        formals_end_position, CHECK_OK_VALUE(kPreParseSuccess));
  }

  Expect(Token::LBRACE, CHECK_OK_VALUE(kPreParseSuccess));
  DeclarationScope* inner_scope = function_scope;
  LazyParsingResult result;

  if (!formals.is_simple) {
    inner_scope = NewVarblockScope();
    inner_scope->set_start_position(scanner()->location().beg_pos);
  }

  {
    BlockState block_state(&scope_, inner_scope);
    result = ParseStatementListAndLogFunction(&formals, may_abort, ok);
  }

  if (!formals.is_simple) {
    BuildParameterInitializationBlock(formals, ok);

    if (is_sloppy(inner_scope->language_mode())) {
      inner_scope->HoistSloppyBlockFunctions(nullptr);
    }

    SetLanguageMode(function_scope, inner_scope->language_mode());
    inner_scope->set_end_position(scanner()->peek_location().end_pos);
    inner_scope->FinalizeBlockScope();
  } else {
    if (is_sloppy(function_scope->language_mode())) {
      function_scope->HoistSloppyBlockFunctions(nullptr);
    }
  }

  if (!IsArrowFunction(kind) && track_unresolved_variables_) {
    // Declare arguments after parsing the function since lexical 'arguments'
    // masks the arguments object. Declare arguments before declaring the
    // function var since the arguments object masks 'function arguments'.
    function_scope->DeclareArguments(ast_value_factory());

    if (FLAG_experimental_preparser_scope_analysis &&
        preparsed_scope_data_ != nullptr) {
      // We're not going to skip this function, but it might contain skippable
      // functions inside it.
      preparsed_scope_data_->AddFunction(
          scope()->start_position(),
          PreParseData::FunctionData(
              scanner()->peek_location().end_pos, scope()->num_parameters(),
              GetLastFunctionLiteralId(), scope()->language_mode(),
              scope()->AsDeclarationScope()->uses_super_property()));
    }
  }

  use_counts_ = nullptr;
  track_unresolved_variables_ = false;

  if (result == kLazyParsingAborted) {
    return kPreParseAbort;
  } else if (stack_overflow()) {
    return kPreParseStackOverflow;
  } else if (!*ok) {
    DCHECK(pending_error_handler_->has_pending_error());
  } else {
    DCHECK_EQ(Token::RBRACE, scanner()->peek());

    if (!IsArrowFunction(kind)) {
      // Validate parameter names. We can do this only after parsing the
      // function, since the function can declare itself strict.
      const bool allow_duplicate_parameters =
          is_sloppy(function_scope->language_mode()) && formals.is_simple &&
          !IsConciseMethod(kind);
      ValidateFormalParameters(function_scope->language_mode(),
                               allow_duplicate_parameters,
                               CHECK_OK_VALUE(kPreParseSuccess));
    }

    if (is_strict(function_scope->language_mode())) {
      int end_pos = scanner()->location().end_pos;
      CheckStrictOctalLiteral(function_scope->start_position(), end_pos, ok);
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
  const RuntimeCallStats::CounterId counters[2][2] = {
      {&RuntimeCallStats::PreParseBackgroundNoVariableResolution,
       &RuntimeCallStats::PreParseNoVariableResolution},
      {&RuntimeCallStats::PreParseBackgroundWithVariableResolution,
       &RuntimeCallStats::PreParseWithVariableResolution}};
  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_,
      counters[track_unresolved_variables_][parsing_on_main_thread_]);

  DeclarationScope* function_scope = NewFunctionScope(kind);
  function_scope->SetLanguageMode(language_mode);
  FunctionState function_state(&function_state_, &scope_, function_scope);
  DuplicateFinder duplicate_finder;
  ExpressionClassifier formals_classifier(this, &duplicate_finder);
  int func_id = GetNextFunctionLiteralId();

  Expect(Token::LPAREN, CHECK_OK);
  int start_position = scanner()->location().beg_pos;
  function_scope->set_start_position(start_position);
  PreParserFormalParameters formals(function_scope);
  ParseFormalParameterList(&formals, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  int formals_end_position = scanner()->location().end_pos;

  CheckArityRestrictions(formals.arity, kind, formals.has_rest, start_position,
                         formals_end_position, CHECK_OK);

  Expect(Token::LBRACE, CHECK_OK);

  // Parse function body.
  PreParserStatementList body;
  int pos = function_token_pos == kNoSourcePosition ? peek_position()
                                                    : function_token_pos;
  ParseFunctionBody(body, function_name, pos, formals, kind, function_type,
                    CHECK_OK);

  // Parsing the body may change the language mode in our scope.
  language_mode = function_scope->language_mode();

  if (is_sloppy(language_mode)) {
    function_scope->HoistSloppyBlockFunctions(nullptr);
  }

  // Validate name and parameter names. We can do this only after parsing the
  // function, since the function can declare itself strict.
  CheckFunctionName(language_mode, function_name, function_name_validity,
                    function_name_location, CHECK_OK);
  const bool allow_duplicate_parameters =
      is_sloppy(language_mode) && formals.is_simple && !IsConciseMethod(kind);
  ValidateFormalParameters(language_mode, allow_duplicate_parameters, CHECK_OK);

  int end_position = scanner()->location().end_pos;
  if (is_strict(language_mode)) {
    CheckStrictOctalLiteral(start_position, end_position, CHECK_OK);
  }

  if (FLAG_experimental_preparser_scope_analysis &&
      track_unresolved_variables_ && preparsed_scope_data_ != nullptr) {
    preparsed_scope_data_->AddSkippableFunction(
        start_position,
        PreParseData::FunctionData(
            end_position, scope()->num_parameters(),
            GetLastFunctionLiteralId() - func_id, scope()->language_mode(),
            scope()->AsDeclarationScope()->uses_super_property()));
  }
  if (FLAG_trace_preparse) {
    PrintF("  [%s]: %i-%i\n",
           track_unresolved_variables_ ? "Preparse resolution"
                                       : "Preparse no-resolution",
           function_scope->start_position(), function_scope->end_position());
  }

  return Expression::Default();
}

PreParser::LazyParsingResult PreParser::ParseStatementListAndLogFunction(
    PreParserFormalParameters* formals, bool may_abort, bool* ok) {
  PreParserStatementList body;
  LazyParsingResult result = ParseStatementList(
      body, Token::RBRACE, may_abort, CHECK_OK_VALUE(kLazyParsingComplete));
  if (result == kLazyParsingAborted) return result;

  // Position right after terminal '}'.
  DCHECK_EQ(Token::RBRACE, scanner()->peek());
  int body_end = scanner()->peek_location().end_pos;
  DCHECK_EQ(this->scope()->is_function_scope(), formals->is_simple);
  log_.LogFunction(body_end, formals->num_parameters(),
                   GetLastFunctionLiteralId());
  return kLazyParsingComplete;
}

PreParserExpression PreParser::ExpressionFromIdentifier(
    PreParserIdentifier name, int start_position, InferName infer) {
  VariableProxy* proxy = nullptr;
  if (track_unresolved_variables_) {
    DCHECK_NOT_NULL(name.string_);
    proxy = scope()->NewUnresolved(factory()->ast_node_factory(), name.string_,
                                   start_position, NORMAL_VARIABLE);
  }
  return PreParserExpression::FromIdentifier(name, proxy, zone());
}

void PreParser::DeclareAndInitializeVariables(
    PreParserStatement block,
    const DeclarationDescriptor* declaration_descriptor,
    const DeclarationParsingResult::Declaration* declaration,
    ZoneList<const AstRawString*>* names, bool* ok) {
  if (declaration->pattern.variables_ != nullptr) {
    DCHECK(FLAG_lazy_inner_functions);
    DCHECK(track_unresolved_variables_);
    for (auto variable : *(declaration->pattern.variables_)) {
      declaration_descriptor->scope->RemoveUnresolved(variable);
      Variable* var = scope()->DeclareVariableName(
          variable->raw_name(), declaration_descriptor->mode);
      if (FLAG_experimental_preparser_scope_analysis) {
        MarkLoopVariableAsAssigned(declaration_descriptor->scope, var);
        // This is only necessary if there is an initializer, but we don't have
        // that information here.  Consequently, the preparser sometimes says
        // maybe-assigned where the parser (correctly) says never-assigned.
      }
      if (names) {
        names->Add(variable->raw_name(), zone());
      }
    }
  }
}

#undef CHECK_OK
#undef CHECK_OK_CUSTOM


}  // namespace internal
}  // namespace v8
