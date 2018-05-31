// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parser.h"

#include <algorithm>
#include <memory>

#include "src/api.h"
#include "src/ast/ast-function-literal-id-reindexer.h"
#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/ast.h"
#include "src/bailout-reason.h"
#include "src/base/platform/platform.h"
#include "src/char-predicates-inl.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/log.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/parsing/duplicate-finder.h"
#include "src/parsing/expression-scope-reparenter.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/rewriter.h"
#include "src/runtime/runtime.h"
#include "src/string-stream.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {



// Helper for putting parts of the parse results into a temporary zone when
// parsing inner function bodies.
class DiscardableZoneScope {
 public:
  DiscardableZoneScope(Parser* parser, Zone* temp_zone, bool use_temp_zone)
      : fni_(parser->ast_value_factory_, temp_zone),
        parser_(parser),
        prev_fni_(parser->fni_),
        prev_zone_(parser->zone_),
        prev_allow_lazy_(parser->allow_lazy_),
        prev_temp_zoned_(parser->temp_zoned_) {
    if (use_temp_zone) {
      DCHECK(!parser_->temp_zoned_);
      parser_->allow_lazy_ = false;
      parser_->temp_zoned_ = true;
      parser_->fni_ = &fni_;
      parser_->zone_ = temp_zone;
      parser_->factory()->set_zone(temp_zone);
      if (parser_->reusable_preparser_ != nullptr) {
        parser_->reusable_preparser_->zone_ = temp_zone;
        parser_->reusable_preparser_->factory()->set_zone(temp_zone);
      }
    }
  }
  void Reset() {
    parser_->fni_ = prev_fni_;
    parser_->zone_ = prev_zone_;
    parser_->factory()->set_zone(prev_zone_);
    parser_->allow_lazy_ = prev_allow_lazy_;
    parser_->temp_zoned_ = prev_temp_zoned_;
    if (parser_->reusable_preparser_ != nullptr) {
      parser_->reusable_preparser_->zone_ = prev_zone_;
      parser_->reusable_preparser_->factory()->set_zone(prev_zone_);
    }
  }
  ~DiscardableZoneScope() { Reset(); }

 private:
  FuncNameInferrer fni_;
  Parser* parser_;
  FuncNameInferrer* prev_fni_;
  Zone* prev_zone_;
  bool prev_allow_lazy_;
  bool prev_temp_zoned_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableZoneScope);
};

FunctionLiteral* Parser::DefaultConstructor(const AstRawString* name,
                                            bool call_super, int pos,
                                            int end_pos) {
  int expected_property_count = -1;
  const int parameter_count = 0;

  FunctionKind kind = call_super ? FunctionKind::kDefaultDerivedConstructor
                                 : FunctionKind::kDefaultBaseConstructor;
  DeclarationScope* function_scope = NewFunctionScope(kind);
  SetLanguageMode(function_scope, LanguageMode::kStrict);
  // Set start and end position to the same value
  function_scope->set_start_position(pos);
  function_scope->set_end_position(pos);
  ZoneList<Statement*>* body = nullptr;

  {
    FunctionState function_state(&function_state_, &scope_, function_scope);

    body = new (zone()) ZoneList<Statement*>(call_super ? 2 : 1, zone());
    if (call_super) {
      // Create a SuperCallReference and handle in BytecodeGenerator.
      auto constructor_args_name = ast_value_factory()->empty_string();
      bool is_duplicate;
      bool is_rest = true;
      bool is_optional = false;
      Variable* constructor_args = function_scope->DeclareParameter(
          constructor_args_name, TEMPORARY, is_optional, is_rest, &is_duplicate,
          ast_value_factory(), pos);

      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(1, zone());
      Spread* spread_args = factory()->NewSpread(
          factory()->NewVariableProxy(constructor_args), pos, pos);

      args->Add(spread_args, zone());
      Expression* super_call_ref = NewSuperCallReference(pos);
      Expression* call = factory()->NewCall(super_call_ref, args, pos);
      body->Add(factory()->NewReturnStatement(call, pos), zone());
    }

    expected_property_count = function_state.expected_property_count();
  }

  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      name, function_scope, body, expected_property_count, parameter_count,
      parameter_count, FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::kAnonymousExpression, default_eager_compile_hint(), pos,
      true, GetNextFunctionLiteralId());
  return function_literal;
}

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

#define CHECK_OK CHECK_OK_VALUE(nullptr)
#define CHECK_OK_VOID CHECK_OK_VALUE(this->Void())

#define CHECK_FAILED /**/); \
  if (failed_) return nullptr;  \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

// ----------------------------------------------------------------------------
// Implementation of Parser

bool Parser::ShortcutNumericLiteralBinaryExpression(Expression** x,
                                                    Expression* y,
                                                    Token::Value op, int pos) {
  if ((*x)->IsNumberLiteral() && y->IsNumberLiteral()) {
    double x_val = (*x)->AsLiteral()->AsNumber();
    double y_val = y->AsLiteral()->AsNumber();
    switch (op) {
      case Token::ADD:
        *x = factory()->NewNumberLiteral(x_val + y_val, pos);
        return true;
      case Token::SUB:
        *x = factory()->NewNumberLiteral(x_val - y_val, pos);
        return true;
      case Token::MUL:
        *x = factory()->NewNumberLiteral(x_val * y_val, pos);
        return true;
      case Token::DIV:
        *x = factory()->NewNumberLiteral(x_val / y_val, pos);
        return true;
      case Token::BIT_OR: {
        int value = DoubleToInt32(x_val) | DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::BIT_AND: {
        int value = DoubleToInt32(x_val) & DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::BIT_XOR: {
        int value = DoubleToInt32(x_val) ^ DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SHL: {
        int value = DoubleToInt32(x_val) << (DoubleToInt32(y_val) & 0x1F);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SHR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1F;
        uint32_t value = DoubleToUint32(x_val) >> shift;
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SAR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1F;
        int value = ArithmeticShiftRight(DoubleToInt32(x_val), shift);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::EXP: {
        double value = Pow(x_val, y_val);
        int int_value = static_cast<int>(value);
        *x = factory()->NewNumberLiteral(
            int_value == value && value != -0.0 ? int_value : value, pos);
        return true;
      }
      default:
        break;
    }
  }
  return false;
}

bool Parser::CollapseNaryExpression(Expression** x, Expression* y,
                                    Token::Value op, int pos,
                                    const SourceRange& range) {
  // Filter out unsupported ops.
  if (!Token::IsBinaryOp(op) || op == Token::EXP) return false;

  // Convert *x into an nary operation with the given op, returning false if
  // this is not possible.
  NaryOperation* nary = nullptr;
  if ((*x)->IsBinaryOperation()) {
    BinaryOperation* binop = (*x)->AsBinaryOperation();
    if (binop->op() != op) return false;

    nary = factory()->NewNaryOperation(op, binop->left(), 2);
    nary->AddSubsequent(binop->right(), binop->position());
    ConvertBinaryToNaryOperationSourceRange(binop, nary);
    *x = nary;
  } else if ((*x)->IsNaryOperation()) {
    nary = (*x)->AsNaryOperation();
    if (nary->op() != op) return false;
  } else {
    return false;
  }

  // Append our current expression to the nary operation.
  // TODO(leszeks): Do some literal collapsing here if we're appending Smi or
  // String literals.
  nary->AddSubsequent(y, pos);
  AppendNaryOperationSourceRange(nary, range);

  return true;
}

Expression* Parser::BuildUnaryExpression(Expression* expression,
                                         Token::Value op, int pos) {
  DCHECK_NOT_NULL(expression);
  const Literal* literal = expression->AsLiteral();
  if (literal != nullptr) {
    if (op == Token::NOT) {
      // Convert the literal to a boolean condition and negate it.
      return factory()->NewBooleanLiteral(literal->ToBooleanIsFalse(), pos);
    } else if (literal->IsNumberLiteral()) {
      // Compute some expressions involving only number literals.
      double value = literal->AsNumber();
      switch (op) {
        case Token::ADD:
          return expression;
        case Token::SUB:
          return factory()->NewNumberLiteral(-value, pos);
        case Token::BIT_NOT:
          return factory()->NewNumberLiteral(~DoubleToInt32(value), pos);
        default:
          break;
      }
    }
  }
  return factory()->NewUnaryOperation(op, expression, pos);
}

Expression* Parser::NewThrowError(Runtime::FunctionId id,
                                  MessageTemplate::Template message,
                                  const AstRawString* arg, int pos) {
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(factory()->NewSmiLiteral(message, pos), zone());
  args->Add(factory()->NewStringLiteral(arg, pos), zone());
  CallRuntime* call_constructor = factory()->NewCallRuntime(id, args, pos);
  return factory()->NewThrow(call_constructor, pos);
}

Expression* Parser::NewSuperPropertyReference(int pos) {
  // this_function[home_object_symbol]
  VariableProxy* this_function_proxy =
      NewUnresolved(ast_value_factory()->this_function_string(), pos);
  Expression* home_object_symbol_literal = factory()->NewSymbolLiteral(
      AstSymbol::kHomeObjectSymbol, kNoSourcePosition);
  Expression* home_object = factory()->NewProperty(
      this_function_proxy, home_object_symbol_literal, pos);
  return factory()->NewSuperPropertyReference(
      ThisExpression(pos)->AsVariableProxy(), home_object, pos);
}

Expression* Parser::NewSuperCallReference(int pos) {
  VariableProxy* new_target_proxy =
      NewUnresolved(ast_value_factory()->new_target_string(), pos);
  VariableProxy* this_function_proxy =
      NewUnresolved(ast_value_factory()->this_function_string(), pos);
  return factory()->NewSuperCallReference(
      ThisExpression(pos)->AsVariableProxy(), new_target_proxy,
      this_function_proxy, pos);
}

Expression* Parser::NewTargetExpression(int pos) {
  auto proxy = NewUnresolved(ast_value_factory()->new_target_string(), pos);
  proxy->set_is_new_target();
  return proxy;
}

Expression* Parser::ImportMetaExpression(int pos) {
  return factory()->NewCallRuntime(
      Runtime::kInlineGetImportMetaObject,
      new (zone()) ZoneList<Expression*>(0, zone()), pos);
}

Literal* Parser::ExpressionFromLiteral(Token::Value token, int pos) {
  switch (token) {
    case Token::NULL_LITERAL:
      return factory()->NewNullLiteral(pos);
    case Token::TRUE_LITERAL:
      return factory()->NewBooleanLiteral(true, pos);
    case Token::FALSE_LITERAL:
      return factory()->NewBooleanLiteral(false, pos);
    case Token::SMI: {
      uint32_t value = scanner()->smi_value();
      return factory()->NewSmiLiteral(value, pos);
    }
    case Token::NUMBER: {
      double value = scanner()->DoubleValue();
      return factory()->NewNumberLiteral(value, pos);
    }
    case Token::BIGINT:
      return factory()->NewBigIntLiteral(
          AstBigInt(scanner()->CurrentLiteralAsCString(zone())), pos);
    default:
      DCHECK(false);
  }
  return nullptr;
}

Expression* Parser::NewV8Intrinsic(const AstRawString* name,
                                   ZoneList<Expression*>* args, int pos,
                                   bool* ok) {
  if (extension_ != nullptr) {
    // The extension structures are only accessible while parsing the
    // very first time, not when reparsing because of lazy compilation.
    GetClosureScope()->ForceEagerCompilation();
  }

  DCHECK(name->is_one_byte());
  const Runtime::Function* function =
      Runtime::FunctionForName(name->raw_data(), name->length());

  if (function != nullptr) {
    // Check for possible name clash.
    DCHECK_EQ(Context::kNotFound,
              Context::IntrinsicIndexForName(name->raw_data(), name->length()));
    // Check for built-in IS_VAR macro.
    if (function->function_id == Runtime::kIS_VAR) {
      DCHECK_EQ(Runtime::RUNTIME, function->intrinsic_type);
      // %IS_VAR(x) evaluates to x if x is a variable,
      // leads to a parse error otherwise.  Could be implemented as an
      // inline function %_IS_VAR(x) to eliminate this special case.
      if (args->length() == 1 && args->at(0)->AsVariableProxy() != nullptr) {
        return args->at(0);
      } else {
        ReportMessage(MessageTemplate::kNotIsvar);
        *ok = false;
        return nullptr;
      }
    }

    // Check that the expected number of arguments are being passed.
    if (function->nargs != -1 && function->nargs != args->length()) {
      ReportMessage(MessageTemplate::kRuntimeWrongNumArgs);
      *ok = false;
      return nullptr;
    }

    return factory()->NewCallRuntime(function, args, pos);
  }

  int context_index =
      Context::IntrinsicIndexForName(name->raw_data(), name->length());

  // Check that the function is defined.
  if (context_index == Context::kNotFound) {
    ReportMessage(MessageTemplate::kNotDefined, name);
    *ok = false;
    return nullptr;
  }

  return factory()->NewCallRuntime(context_index, args, pos);
}

Parser::Parser(ParseInfo* info)
    : ParserBase<Parser>(info->zone(), &scanner_, info->stack_limit(),
                         info->extension(), info->GetOrCreateAstValueFactory(),
                         info->pending_error_handler(),
                         info->runtime_call_stats(), info->logger(),
                         info->script().is_null() ? -1 : info->script()->id(),
                         info->is_module(), true),
      scanner_(info->unicode_cache()),
      reusable_preparser_(nullptr),
      mode_(PARSE_EAGERLY),  // Lazy mode must be set explicitly.
      source_range_map_(info->source_range_map()),
      target_stack_(nullptr),
      total_preparse_skipped_(0),
      temp_zoned_(false),
      consumed_preparsed_scope_data_(info->consumed_preparsed_scope_data()),
      parameters_end_pos_(info->parameters_end_pos()) {
  // Even though we were passed ParseInfo, we should not store it in
  // Parser - this makes sure that Isolate is not accidentally accessed via
  // ParseInfo during background parsing.
  DCHECK_NOT_NULL(info->character_stream());
  // Determine if functions can be lazily compiled. This is necessary to
  // allow some of our builtin JS files to be lazily compiled. These
  // builtins cannot be handled lazily by the parser, since we have to know
  // if a function uses the special natives syntax, which is something the
  // parser records.
  // If the debugger requests compilation for break points, we cannot be
  // aggressive about lazy compilation, because it might trigger compilation
  // of functions without an outer context when setting a breakpoint through
  // Debug::FindSharedFunctionInfoInScript
  // We also compile eagerly for kProduceExhaustiveCodeCache.
  bool can_compile_lazily = FLAG_lazy && !info->is_eager();

  set_default_eager_compile_hint(can_compile_lazily
                                     ? FunctionLiteral::kShouldLazyCompile
                                     : FunctionLiteral::kShouldEagerCompile);
  allow_lazy_ = FLAG_lazy && info->allow_lazy_parsing() && !info->is_native() &&
                info->extension() == nullptr && can_compile_lazily;
  set_allow_natives(FLAG_allow_natives_syntax || info->is_native());
  set_allow_harmony_do_expressions(FLAG_harmony_do_expressions);
  set_allow_harmony_public_fields(FLAG_harmony_public_fields);
  set_allow_harmony_static_fields(FLAG_harmony_static_fields);
  set_allow_harmony_dynamic_import(FLAG_harmony_dynamic_import);
  set_allow_harmony_import_meta(FLAG_harmony_import_meta);
  set_allow_harmony_bigint(FLAG_harmony_bigint);
  set_allow_harmony_numeric_separator(FLAG_harmony_numeric_separator);
  set_allow_harmony_optional_catch_binding(FLAG_harmony_optional_catch_binding);
  set_allow_harmony_private_fields(FLAG_harmony_private_fields);
  for (int feature = 0; feature < v8::Isolate::kUseCounterFeatureCount;
       ++feature) {
    use_counts_[feature] = 0;
  }
}

void Parser::DeserializeScopeChain(
    ParseInfo* info, MaybeHandle<ScopeInfo> maybe_outer_scope_info) {
  // TODO(wingo): Add an outer SCRIPT_SCOPE corresponding to the native
  // context, which will have the "this" binding for script scopes.
  DeclarationScope* script_scope = NewScriptScope();
  info->set_script_scope(script_scope);
  Scope* scope = script_scope;
  Handle<ScopeInfo> outer_scope_info;
  if (maybe_outer_scope_info.ToHandle(&outer_scope_info)) {
    DCHECK(ThreadId::Current().Equals(
        outer_scope_info->GetIsolate()->thread_id()));
    scope = Scope::DeserializeScopeChain(
        zone(), *outer_scope_info, script_scope, ast_value_factory(),
        Scope::DeserializationMode::kScopesOnly);
  }
  original_scope_ = scope;
}

namespace {

void MaybeResetCharacterStream(ParseInfo* info, FunctionLiteral* literal) {
  // Don't reset the character stream if there is an asm.js module since it will
  // be used again by the asm-parser.
  if (!FLAG_stress_validate_asm &&
      (literal == nullptr || !literal->scope()->ContainsAsmModule())) {
    info->ResetCharacterStream();
  }
}

}  // namespace

FunctionLiteral* Parser::ParseProgram(Isolate* isolate, ParseInfo* info) {
  // TODO(bmeurer): We temporarily need to pass allow_nesting = true here,
  // see comment for HistogramTimerScope class.

  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);
  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_, info->is_eval()
                               ? RuntimeCallCounterId::kParseEval
                               : RuntimeCallCounterId::kParseProgram);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.ParseProgram");
  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();
  fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());

  // Initialize parser state.
  DeserializeScopeChain(info, info->maybe_outer_scope_info());

  scanner_.Initialize(info->character_stream(), info->is_module());
  FunctionLiteral* result = DoParseProgram(info);
  MaybeResetCharacterStream(info, result);

  HandleSourceURLComments(isolate, info->script());

  if (V8_UNLIKELY(FLAG_log_function_events) && result != nullptr) {
    double ms = timer.Elapsed().InMillisecondsF();
    const char* event_name = "parse-eval";
    Script* script = *info->script();
    int start = -1;
    int end = -1;
    if (!info->is_eval()) {
      event_name = "parse-script";
      start = 0;
      end = String::cast(script->source())->length();
    }
    LOG(script->GetIsolate(),
        FunctionEvent(event_name, script, -1, ms, start, end, "", 0));
  }
  return result;
}


FunctionLiteral* Parser::DoParseProgram(ParseInfo* info) {
  // Note that this function can be called from the main thread or from a
  // background thread. We should not access anything Isolate / heap dependent
  // via ParseInfo, and also not pass it forward.
  DCHECK_NULL(scope_);
  DCHECK_NULL(target_stack_);

  ParsingModeScope mode(this, allow_lazy_ ? PARSE_LAZILY : PARSE_EAGERLY);
  ResetFunctionLiteralId();
  DCHECK(info->function_literal_id() == FunctionLiteral::kIdTypeTopLevel ||
         info->function_literal_id() == FunctionLiteral::kIdTypeInvalid);

  FunctionLiteral* result = nullptr;
  {
    Scope* outer = original_scope_;
    DCHECK_NOT_NULL(outer);
    if (info->is_eval()) {
      outer = NewEvalScope(outer);
    } else if (parsing_module_) {
      DCHECK_EQ(outer, info->script_scope());
      outer = NewModuleScope(info->script_scope());
    }

    DeclarationScope* scope = outer->AsDeclarationScope();
    scope->set_start_position(0);

    FunctionState function_state(&function_state_, &scope_, scope);
    ZoneList<Statement*>* body = new(zone()) ZoneList<Statement*>(16, zone());
    bool ok = true;
    int beg_pos = scanner()->location().beg_pos;
    if (parsing_module_) {
      DCHECK(info->is_module());
      // Declare the special module parameter.
      auto name = ast_value_factory()->empty_string();
      bool is_duplicate = false;
      bool is_rest = false;
      bool is_optional = false;
      auto var =
          scope->DeclareParameter(name, VAR, is_optional, is_rest,
                                  &is_duplicate, ast_value_factory(), beg_pos);
      DCHECK(!is_duplicate);
      var->AllocateTo(VariableLocation::PARAMETER, 0);

      PrepareGeneratorVariables();
      Expression* initial_yield =
          BuildInitialYield(kNoSourcePosition, kGeneratorFunction);
      body->Add(
          factory()->NewExpressionStatement(initial_yield, kNoSourcePosition),
          zone());

      ParseModuleItemList(body, &ok);
      ok = ok && module()->Validate(this->scope()->AsModuleScope(),
                                    pending_error_handler(), zone());
    } else if (info->is_wrapped_as_function()) {
      ParseWrapped(info, body, scope, zone(), &ok);
    } else {
      // Don't count the mode in the use counters--give the program a chance
      // to enable script-wide strict mode below.
      this->scope()->SetLanguageMode(info->language_mode());
      ParseStatementList(body, Token::EOS, &ok);
    }

    // The parser will peek but not consume EOS.  Our scope logically goes all
    // the way to the EOS, though.
    scope->set_end_position(scanner()->peek_location().beg_pos);

    if (ok && is_strict(language_mode())) {
      CheckStrictOctalLiteral(beg_pos, scanner()->location().end_pos, &ok);
    }
    if (ok && is_sloppy(language_mode())) {
      // TODO(littledan): Function bindings on the global object that modify
      // pre-existing bindings should be made writable, enumerable and
      // nonconfigurable if possible, whereas this code will leave attributes
      // unchanged if the property already exists.
      InsertSloppyBlockFunctionVarBindings(scope);
    }
    if (ok) {
      CheckConflictingVarDeclarations(scope, &ok);
    }

    if (ok && info->parse_restriction() == ONLY_SINGLE_FUNCTION_LITERAL) {
      if (body->length() != 1 ||
          !body->at(0)->IsExpressionStatement() ||
          !body->at(0)->AsExpressionStatement()->
              expression()->IsFunctionLiteral()) {
        ReportMessage(MessageTemplate::kSingleFunctionLiteral);
        ok = false;
      }
    }

    if (ok) {
      RewriteDestructuringAssignments();
      int parameter_count = parsing_module_ ? 1 : 0;
      result = factory()->NewScriptOrEvalFunctionLiteral(
          scope, body, function_state.expected_property_count(),
          parameter_count);
      result->set_suspend_count(function_state.suspend_count());
    }
  }

  info->set_max_function_literal_id(GetLastFunctionLiteralId());

  // Make sure the target stack is empty.
  DCHECK_NULL(target_stack_);

  return result;
}

ZoneList<const AstRawString*>* Parser::PrepareWrappedArguments(ParseInfo* info,
                                                               Zone* zone) {
  DCHECK(parsing_on_main_thread_);
  Handle<FixedArray> arguments(info->script()->wrapped_arguments());
  int arguments_length = arguments->length();
  ZoneList<const AstRawString*>* arguments_for_wrapped_function =
      new (zone) ZoneList<const AstRawString*>(arguments_length, zone);
  for (int i = 0; i < arguments_length; i++) {
    const AstRawString* argument_string = ast_value_factory()->GetString(
        Handle<String>(String::cast(arguments->get(i))));
    arguments_for_wrapped_function->Add(argument_string, zone);
  }
  return arguments_for_wrapped_function;
}

void Parser::ParseWrapped(ParseInfo* info, ZoneList<Statement*>* body,
                          DeclarationScope* outer_scope, Zone* zone, bool* ok) {
  DCHECK(info->is_wrapped_as_function());
  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  // Set function and block state for the outer eval scope.
  DCHECK(outer_scope->is_eval_scope());
  FunctionState function_state(&function_state_, &scope_, outer_scope);

  const AstRawString* function_name = nullptr;
  Scanner::Location location(0, 0);

  ZoneList<const AstRawString*>* arguments_for_wrapped_function =
      PrepareWrappedArguments(info, zone);

  FunctionLiteral* function_literal = ParseFunctionLiteral(
      function_name, location, kSkipFunctionNameCheck, kNormalFunction,
      kNoSourcePosition, FunctionLiteral::kWrapped, LanguageMode::kSloppy,
      arguments_for_wrapped_function, CHECK_OK_VOID);

  Statement* return_statement = factory()->NewReturnStatement(
      function_literal, kNoSourcePosition, kNoSourcePosition);
  body->Add(return_statement, zone);
}

FunctionLiteral* Parser::ParseFunction(Isolate* isolate, ParseInfo* info,
                                       Handle<SharedFunctionInfo> shared_info) {
  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);
  RuntimeCallTimerScope runtime_timer(runtime_call_stats_,
                                      RuntimeCallCounterId::kParseFunction);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.ParseFunction");
  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();

  DeserializeScopeChain(info, info->maybe_outer_scope_info());
  DCHECK_EQ(factory()->zone(), info->zone());

  // Initialize parser state.
  Handle<String> name(shared_info->Name());
  info->set_function_name(ast_value_factory()->GetString(name));
  scanner_.Initialize(info->character_stream(), info->is_module());

  FunctionLiteral* result = DoParseFunction(info, info->function_name());
  MaybeResetCharacterStream(info, result);
  if (result != nullptr) {
    Handle<String> inferred_name(shared_info->inferred_name());
    result->set_inferred_name(inferred_name);
  }

  if (V8_UNLIKELY(FLAG_log_function_events) && result != nullptr) {
    double ms = timer.Elapsed().InMillisecondsF();
    // We need to make sure that the debug-name is available.
    ast_value_factory()->Internalize(isolate);
    DeclarationScope* function_scope = result->scope();
    Script* script = *info->script();
    std::unique_ptr<char[]> function_name = result->GetDebugName();
    LOG(script->GetIsolate(),
        FunctionEvent("parse-function", script, -1, ms,
                      function_scope->start_position(),
                      function_scope->end_position(), function_name.get(),
                      strlen(function_name.get())));
  }
  return result;
}

static FunctionLiteral::FunctionType ComputeFunctionType(ParseInfo* info) {
  if (info->is_wrapped_as_function()) {
    return FunctionLiteral::kWrapped;
  } else if (info->is_declaration()) {
    return FunctionLiteral::kDeclaration;
  } else if (info->is_named_expression()) {
    return FunctionLiteral::kNamedExpression;
  } else if (IsConciseMethod(info->function_kind()) ||
             IsAccessorFunction(info->function_kind())) {
    return FunctionLiteral::kAccessorOrMethod;
  }
  return FunctionLiteral::kAnonymousExpression;
}

FunctionLiteral* Parser::DoParseFunction(ParseInfo* info,
                                         const AstRawString* raw_name) {
  DCHECK_NOT_NULL(raw_name);
  DCHECK_NULL(scope_);
  DCHECK_NULL(target_stack_);

  DCHECK(ast_value_factory());
  fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());
  fni_->PushEnclosingName(raw_name);

  ResetFunctionLiteralId();
  DCHECK_LT(0, info->function_literal_id());
  SkipFunctionLiterals(info->function_literal_id() - 1);

  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  // Place holder for the result.
  FunctionLiteral* result = nullptr;

  {
    // Parse the function literal.
    Scope* outer = original_scope_;
    DeclarationScope* outer_function = outer->GetClosureScope();
    DCHECK(outer);
    FunctionState function_state(&function_state_, &scope_, outer_function);
    BlockState block_state(&scope_, outer);
    DCHECK(is_sloppy(outer->language_mode()) ||
           is_strict(info->language_mode()));
    FunctionLiteral::FunctionType function_type = ComputeFunctionType(info);
    FunctionKind kind = info->function_kind();
    bool ok = true;

    if (IsArrowFunction(kind)) {
      if (IsAsyncFunction(kind)) {
        DCHECK(!scanner()->HasAnyLineTerminatorAfterNext());
        if (!Check(Token::ASYNC)) {
          CHECK(stack_overflow());
          return nullptr;
        }
        if (!(peek_any_identifier() || peek() == Token::LPAREN)) {
          CHECK(stack_overflow());
          return nullptr;
        }
      }

      // TODO(adamk): We should construct this scope from the ScopeInfo.
      DeclarationScope* scope = NewFunctionScope(kind);

      // This bit only needs to be explicitly set because we're
      // not passing the ScopeInfo to the Scope constructor.
      SetLanguageMode(scope, info->language_mode());

      scope->set_start_position(info->start_position());
      ExpressionClassifier formals_classifier(this);
      ParserFormalParameters formals(scope);
      // The outer FunctionState should not contain destructuring assignments.
      DCHECK_EQ(0,
                function_state.destructuring_assignments_to_rewrite().length());
      {
        // Parsing patterns as variable reference expression creates
        // NewUnresolved references in current scope. Enter arrow function
        // scope for formal parameter parsing.
        BlockState block_state(&scope_, scope);
        if (Check(Token::LPAREN)) {
          // '(' StrictFormalParameters ')'
          ParseFormalParameterList(&formals, &ok);
          if (ok) ok = Check(Token::RPAREN);
        } else {
          // BindingIdentifier
          ParseFormalParameter(&formals, &ok);
          if (ok) {
            DeclareFormalParameters(formals.scope, formals.params,
                                    formals.is_simple);
          }
        }
      }

      if (ok) {
        if (GetLastFunctionLiteralId() != info->function_literal_id() - 1) {
          // If there were FunctionLiterals in the parameters, we need to
          // renumber them to shift down so the next function literal id for
          // the arrow function is the one requested.
          AstFunctionLiteralIdReindexer reindexer(
              stack_limit_,
              (info->function_literal_id() - 1) - GetLastFunctionLiteralId());
          for (auto p : formals.params) {
            if (p->pattern != nullptr) reindexer.Reindex(p->pattern);
            if (p->initializer != nullptr) reindexer.Reindex(p->initializer);
          }
          ResetFunctionLiteralId();
          SkipFunctionLiterals(info->function_literal_id() - 1);
        }

        // Pass `accept_IN=true` to ParseArrowFunctionLiteral --- This should
        // not be observable, or else the preparser would have failed.
        const bool accept_IN = true;
        // Any destructuring assignments in the current FunctionState
        // actually belong to the arrow function itself.
        const int rewritable_length = 0;
        Expression* expression = ParseArrowFunctionLiteral(
            accept_IN, formals, rewritable_length, &ok);
        if (ok) {
          // Scanning must end at the same position that was recorded
          // previously. If not, parsing has been interrupted due to a stack
          // overflow, at which point the partially parsed arrow function
          // concise body happens to be a valid expression. This is a problem
          // only for arrow functions with single expression bodies, since there
          // is no end token such as "}" for normal functions.
          if (scanner()->location().end_pos == info->end_position()) {
            // The pre-parser saw an arrow function here, so the full parser
            // must produce a FunctionLiteral.
            DCHECK(expression->IsFunctionLiteral());
            result = expression->AsFunctionLiteral();
          } else {
            ok = false;
          }
        }
      }
    } else if (IsDefaultConstructor(kind)) {
      DCHECK_EQ(scope(), outer);
      result = DefaultConstructor(raw_name, IsDerivedConstructor(kind),
                                  info->start_position(), info->end_position());
    } else {
      ZoneList<const AstRawString*>* arguments_for_wrapped_function =
          info->is_wrapped_as_function() ? PrepareWrappedArguments(info, zone())
                                         : nullptr;
      result = ParseFunctionLiteral(
          raw_name, Scanner::Location::invalid(), kSkipFunctionNameCheck, kind,
          kNoSourcePosition, function_type, info->language_mode(),
          arguments_for_wrapped_function, &ok);
    }

    if (ok) {
      result->set_requires_instance_fields_initializer(
          info->requires_instance_fields_initializer());
    }
    // Make sure the results agree.
    DCHECK(ok == (result != nullptr));
  }

  // Make sure the target stack is empty.
  DCHECK_NULL(target_stack_);
  DCHECK_IMPLIES(result,
                 info->function_literal_id() == result->function_literal_id());
  return result;
}

Statement* Parser::ParseModuleItem(bool* ok) {
  // ecma262/#prod-ModuleItem
  // ModuleItem :
  //    ImportDeclaration
  //    ExportDeclaration
  //    StatementListItem

  Token::Value next = peek();

  if (next == Token::EXPORT) {
    return ParseExportDeclaration(ok);
  }

  if (next == Token::IMPORT) {
    // We must be careful not to parse a dynamic import expression as an import
    // declaration. Same for import.meta expressions.
    Token::Value peek_ahead = PeekAhead();
    if ((!allow_harmony_dynamic_import() || peek_ahead != Token::LPAREN) &&
        (!allow_harmony_import_meta() || peek_ahead != Token::PERIOD)) {
      ParseImportDeclaration(CHECK_OK);
      return factory()->NewEmptyStatement(kNoSourcePosition);
    }
  }

  return ParseStatementListItem(ok);
}


void Parser::ParseModuleItemList(ZoneList<Statement*>* body, bool* ok) {
  // ecma262/#prod-Module
  // Module :
  //    ModuleBody?
  //
  // ecma262/#prod-ModuleItemList
  // ModuleBody :
  //    ModuleItem*

  DCHECK(scope()->is_module_scope());
  while (peek() != Token::EOS) {
    Statement* stat = ParseModuleItem(CHECK_OK_VOID);
    if (stat && !stat->IsEmpty()) {
      body->Add(stat, zone());
    }
  }
}


const AstRawString* Parser::ParseModuleSpecifier(bool* ok) {
  // ModuleSpecifier :
  //    StringLiteral

  Expect(Token::STRING, CHECK_OK);
  return GetSymbol();
}


void Parser::ParseExportClause(ZoneList<const AstRawString*>* export_names,
                               ZoneList<Scanner::Location>* export_locations,
                               ZoneList<const AstRawString*>* local_names,
                               Scanner::Location* reserved_loc, bool* ok) {
  // ExportClause :
  //   '{' '}'
  //   '{' ExportsList '}'
  //   '{' ExportsList ',' '}'
  //
  // ExportsList :
  //   ExportSpecifier
  //   ExportsList ',' ExportSpecifier
  //
  // ExportSpecifier :
  //   IdentifierName
  //   IdentifierName 'as' IdentifierName

  Expect(Token::LBRACE, CHECK_OK_VOID);

  Token::Value name_tok;
  while ((name_tok = peek()) != Token::RBRACE) {
    // Keep track of the first reserved word encountered in case our
    // caller needs to report an error.
    if (!reserved_loc->IsValid() &&
        !Token::IsIdentifier(name_tok, LanguageMode::kStrict, false,
                             parsing_module_)) {
      *reserved_loc = scanner()->location();
    }
    const AstRawString* local_name = ParseIdentifierName(CHECK_OK_VOID);
    const AstRawString* export_name = nullptr;
    Scanner::Location location = scanner()->location();
    if (CheckContextualKeyword(Token::AS)) {
      export_name = ParseIdentifierName(CHECK_OK_VOID);
      // Set the location to the whole "a as b" string, so that it makes sense
      // both for errors due to "a" and for errors due to "b".
      location.end_pos = scanner()->location().end_pos;
    }
    if (export_name == nullptr) {
      export_name = local_name;
    }
    export_names->Add(export_name, zone());
    local_names->Add(local_name, zone());
    export_locations->Add(location, zone());
    if (peek() == Token::RBRACE) break;
    Expect(Token::COMMA, CHECK_OK_VOID);
  }

  Expect(Token::RBRACE, CHECK_OK_VOID);
}


ZoneList<const Parser::NamedImport*>* Parser::ParseNamedImports(
    int pos, bool* ok) {
  // NamedImports :
  //   '{' '}'
  //   '{' ImportsList '}'
  //   '{' ImportsList ',' '}'
  //
  // ImportsList :
  //   ImportSpecifier
  //   ImportsList ',' ImportSpecifier
  //
  // ImportSpecifier :
  //   BindingIdentifier
  //   IdentifierName 'as' BindingIdentifier

  Expect(Token::LBRACE, CHECK_OK);

  auto result = new (zone()) ZoneList<const NamedImport*>(1, zone());
  while (peek() != Token::RBRACE) {
    const AstRawString* import_name = ParseIdentifierName(CHECK_OK);
    const AstRawString* local_name = import_name;
    Scanner::Location location = scanner()->location();
    // In the presence of 'as', the left-side of the 'as' can
    // be any IdentifierName. But without 'as', it must be a valid
    // BindingIdentifier.
    if (CheckContextualKeyword(Token::AS)) {
      local_name = ParseIdentifierName(CHECK_OK);
    }
    if (!Token::IsIdentifier(scanner()->current_token(), LanguageMode::kStrict,
                             false, parsing_module_)) {
      *ok = false;
      ReportMessage(MessageTemplate::kUnexpectedReserved);
      return nullptr;
    } else if (IsEvalOrArguments(local_name)) {
      *ok = false;
      ReportMessage(MessageTemplate::kStrictEvalArguments);
      return nullptr;
    }

    DeclareVariable(local_name, CONST, kNeedsInitialization, position(),
                    CHECK_OK);

    NamedImport* import =
        new (zone()) NamedImport(import_name, local_name, location);
    result->Add(import, zone());

    if (peek() == Token::RBRACE) break;
    Expect(Token::COMMA, CHECK_OK);
  }

  Expect(Token::RBRACE, CHECK_OK);
  return result;
}


void Parser::ParseImportDeclaration(bool* ok) {
  // ImportDeclaration :
  //   'import' ImportClause 'from' ModuleSpecifier ';'
  //   'import' ModuleSpecifier ';'
  //
  // ImportClause :
  //   ImportedDefaultBinding
  //   NameSpaceImport
  //   NamedImports
  //   ImportedDefaultBinding ',' NameSpaceImport
  //   ImportedDefaultBinding ',' NamedImports
  //
  // NameSpaceImport :
  //   '*' 'as' ImportedBinding

  int pos = peek_position();
  Expect(Token::IMPORT, CHECK_OK_VOID);

  Token::Value tok = peek();

  // 'import' ModuleSpecifier ';'
  if (tok == Token::STRING) {
    Scanner::Location specifier_loc = scanner()->peek_location();
    const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK_VOID);
    ExpectSemicolon(CHECK_OK_VOID);
    module()->AddEmptyImport(module_specifier, specifier_loc);
    return;
  }

  // Parse ImportedDefaultBinding if present.
  const AstRawString* import_default_binding = nullptr;
  Scanner::Location import_default_binding_loc;
  if (tok != Token::MUL && tok != Token::LBRACE) {
    import_default_binding =
        ParseIdentifier(kDontAllowRestrictedIdentifiers, CHECK_OK_VOID);
    import_default_binding_loc = scanner()->location();
    DeclareVariable(import_default_binding, CONST, kNeedsInitialization, pos,
                    CHECK_OK_VOID);
  }

  // Parse NameSpaceImport or NamedImports if present.
  const AstRawString* module_namespace_binding = nullptr;
  Scanner::Location module_namespace_binding_loc;
  const ZoneList<const NamedImport*>* named_imports = nullptr;
  if (import_default_binding == nullptr || Check(Token::COMMA)) {
    switch (peek()) {
      case Token::MUL: {
        Consume(Token::MUL);
        ExpectContextualKeyword(Token::AS, CHECK_OK_VOID);
        module_namespace_binding =
            ParseIdentifier(kDontAllowRestrictedIdentifiers, CHECK_OK_VOID);
        module_namespace_binding_loc = scanner()->location();
        DeclareVariable(module_namespace_binding, CONST, kCreatedInitialized,
                        pos, CHECK_OK_VOID);
        break;
      }

      case Token::LBRACE:
        named_imports = ParseNamedImports(pos, CHECK_OK_VOID);
        break;

      default:
        *ok = false;
        ReportUnexpectedToken(scanner()->current_token());
        return;
    }
  }

  ExpectContextualKeyword(Token::FROM, CHECK_OK_VOID);
  Scanner::Location specifier_loc = scanner()->peek_location();
  const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK_VOID);
  ExpectSemicolon(CHECK_OK_VOID);

  // Now that we have all the information, we can make the appropriate
  // declarations.

  // TODO(neis): Would prefer to call DeclareVariable for each case below rather
  // than above and in ParseNamedImports, but then a possible error message
  // would point to the wrong location.  Maybe have a DeclareAt version of
  // Declare that takes a location?

  if (module_namespace_binding != nullptr) {
    module()->AddStarImport(module_namespace_binding, module_specifier,
                            module_namespace_binding_loc, specifier_loc,
                            zone());
  }

  if (import_default_binding != nullptr) {
    module()->AddImport(ast_value_factory()->default_string(),
                        import_default_binding, module_specifier,
                        import_default_binding_loc, specifier_loc, zone());
  }

  if (named_imports != nullptr) {
    if (named_imports->length() == 0) {
      module()->AddEmptyImport(module_specifier, specifier_loc);
    } else {
      for (int i = 0; i < named_imports->length(); ++i) {
        const NamedImport* import = named_imports->at(i);
        module()->AddImport(import->import_name, import->local_name,
                            module_specifier, import->location, specifier_loc,
                            zone());
      }
    }
  }
}


Statement* Parser::ParseExportDefault(bool* ok) {
  //  Supports the following productions, starting after the 'default' token:
  //    'export' 'default' HoistableDeclaration
  //    'export' 'default' ClassDeclaration
  //    'export' 'default' AssignmentExpression[In] ';'

  Expect(Token::DEFAULT, CHECK_OK);
  Scanner::Location default_loc = scanner()->location();

  ZoneList<const AstRawString*> local_names(1, zone());
  Statement* result = nullptr;
  switch (peek()) {
    case Token::FUNCTION:
      result = ParseHoistableDeclaration(&local_names, true, CHECK_OK);
      break;

    case Token::CLASS:
      Consume(Token::CLASS);
      result = ParseClassDeclaration(&local_names, true, CHECK_OK);
      break;

    case Token::ASYNC:
      if (PeekAhead() == Token::FUNCTION &&
          !scanner()->HasAnyLineTerminatorAfterNext()) {
        Consume(Token::ASYNC);
        result = ParseAsyncFunctionDeclaration(&local_names, true, CHECK_OK);
        break;
      }
      V8_FALLTHROUGH;

    default: {
      int pos = position();
      ExpressionClassifier classifier(this);
      Expression* value = ParseAssignmentExpression(true, CHECK_OK);
      ValidateExpression(CHECK_OK);
      SetFunctionName(value, ast_value_factory()->default_string());

      const AstRawString* local_name =
          ast_value_factory()->star_default_star_string();
      local_names.Add(local_name, zone());

      // It's fine to declare this as CONST because the user has no way of
      // writing to it.
      Declaration* decl = DeclareVariable(local_name, CONST, pos, CHECK_OK);
      decl->proxy()->var()->set_initializer_position(position());

      Assignment* assignment = factory()->NewAssignment(
          Token::INIT, decl->proxy(), value, kNoSourcePosition);
      result = IgnoreCompletion(
          factory()->NewExpressionStatement(assignment, kNoSourcePosition));

      ExpectSemicolon(CHECK_OK);
      break;
    }
  }

  DCHECK_EQ(local_names.length(), 1);
  module()->AddExport(local_names.first(),
                      ast_value_factory()->default_string(), default_loc,
                      zone());

  DCHECK_NOT_NULL(result);
  return result;
}

Statement* Parser::ParseExportDeclaration(bool* ok) {
  // ExportDeclaration:
  //    'export' '*' 'from' ModuleSpecifier ';'
  //    'export' ExportClause ('from' ModuleSpecifier)? ';'
  //    'export' VariableStatement
  //    'export' Declaration
  //    'export' 'default' ... (handled in ParseExportDefault)

  Expect(Token::EXPORT, CHECK_OK);
  int pos = position();

  Statement* result = nullptr;
  ZoneList<const AstRawString*> names(1, zone());
  Scanner::Location loc = scanner()->peek_location();
  switch (peek()) {
    case Token::DEFAULT:
      return ParseExportDefault(ok);

    case Token::MUL: {
      Consume(Token::MUL);
      loc = scanner()->location();
      ExpectContextualKeyword(Token::FROM, CHECK_OK);
      Scanner::Location specifier_loc = scanner()->peek_location();
      const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK);
      ExpectSemicolon(CHECK_OK);
      module()->AddStarExport(module_specifier, loc, specifier_loc, zone());
      return factory()->NewEmptyStatement(pos);
    }

    case Token::LBRACE: {
      // There are two cases here:
      //
      // 'export' ExportClause ';'
      // and
      // 'export' ExportClause FromClause ';'
      //
      // In the first case, the exported identifiers in ExportClause must
      // not be reserved words, while in the latter they may be. We
      // pass in a location that gets filled with the first reserved word
      // encountered, and then throw a SyntaxError if we are in the
      // non-FromClause case.
      Scanner::Location reserved_loc = Scanner::Location::invalid();
      ZoneList<const AstRawString*> export_names(1, zone());
      ZoneList<Scanner::Location> export_locations(1, zone());
      ZoneList<const AstRawString*> original_names(1, zone());
      ParseExportClause(&export_names, &export_locations, &original_names,
                        &reserved_loc, CHECK_OK);
      const AstRawString* module_specifier = nullptr;
      Scanner::Location specifier_loc;
      if (CheckContextualKeyword(Token::FROM)) {
        specifier_loc = scanner()->peek_location();
        module_specifier = ParseModuleSpecifier(CHECK_OK);
      } else if (reserved_loc.IsValid()) {
        // No FromClause, so reserved words are invalid in ExportClause.
        *ok = false;
        ReportMessageAt(reserved_loc, MessageTemplate::kUnexpectedReserved);
        return nullptr;
      }
      ExpectSemicolon(CHECK_OK);
      const int length = export_names.length();
      DCHECK_EQ(length, original_names.length());
      DCHECK_EQ(length, export_locations.length());
      if (module_specifier == nullptr) {
        for (int i = 0; i < length; ++i) {
          module()->AddExport(original_names[i], export_names[i],
                              export_locations[i], zone());
        }
      } else if (length == 0) {
        module()->AddEmptyImport(module_specifier, specifier_loc);
      } else {
        for (int i = 0; i < length; ++i) {
          module()->AddExport(original_names[i], export_names[i],
                              module_specifier, export_locations[i],
                              specifier_loc, zone());
        }
      }
      return factory()->NewEmptyStatement(pos);
    }

    case Token::FUNCTION:
      result = ParseHoistableDeclaration(&names, false, CHECK_OK);
      break;

    case Token::CLASS:
      Consume(Token::CLASS);
      result = ParseClassDeclaration(&names, false, CHECK_OK);
      break;

    case Token::VAR:
    case Token::LET:
    case Token::CONST:
      result = ParseVariableStatement(kStatementListItem, &names, CHECK_OK);
      break;

    case Token::ASYNC:
      // TODO(neis): Why don't we have the same check here as in
      // ParseStatementListItem?
      Consume(Token::ASYNC);
      result = ParseAsyncFunctionDeclaration(&names, false, CHECK_OK);
      break;

    default:
      *ok = false;
      ReportUnexpectedToken(scanner()->current_token());
      return nullptr;
  }
  loc.end_pos = scanner()->location().end_pos;

  ModuleDescriptor* descriptor = module();
  for (int i = 0; i < names.length(); ++i) {
    descriptor->AddExport(names[i], names[i], loc, zone());
  }

  DCHECK_NOT_NULL(result);
  return result;
}

VariableProxy* Parser::NewUnresolved(const AstRawString* name, int begin_pos,
                                     VariableKind kind) {
  return scope()->NewUnresolved(factory(), name, begin_pos, kind);
}

VariableProxy* Parser::NewUnresolved(const AstRawString* name) {
  return scope()->NewUnresolved(factory(), name, scanner()->location().beg_pos);
}

Declaration* Parser::DeclareVariable(const AstRawString* name,
                                     VariableMode mode, int pos, bool* ok) {
  return DeclareVariable(name, mode, Variable::DefaultInitializationFlag(mode),
                         pos, ok);
}

Declaration* Parser::DeclareVariable(const AstRawString* name,
                                     VariableMode mode, InitializationFlag init,
                                     int pos, bool* ok) {
  DCHECK_NOT_NULL(name);
  VariableProxy* proxy = factory()->NewVariableProxy(
      name, NORMAL_VARIABLE, scanner()->location().beg_pos);
  Declaration* declaration;
  if (mode == VAR && !scope()->is_declaration_scope()) {
    DCHECK(scope()->is_block_scope() || scope()->is_with_scope());
    declaration = factory()->NewNestedVariableDeclaration(proxy, scope(), pos);
  } else {
    declaration = factory()->NewVariableDeclaration(proxy, pos);
  }
  Declare(declaration, DeclarationDescriptor::NORMAL, mode, init, ok, nullptr,
          scanner()->location().end_pos);
  if (!*ok) return nullptr;
  return declaration;
}

Variable* Parser::Declare(Declaration* declaration,
                          DeclarationDescriptor::Kind declaration_kind,
                          VariableMode mode, InitializationFlag init, bool* ok,
                          Scope* scope, int var_end_pos) {
  if (scope == nullptr) {
    scope = this->scope();
  }
  bool sloppy_mode_block_scope_function_redefinition = false;
  Variable* variable = scope->DeclareVariable(
      declaration, mode, init, &sloppy_mode_block_scope_function_redefinition,
      ok);
  if (!*ok) {
    // If we only have the start position of a proxy, we can't highlight the
    // whole variable name.  Pretend its length is 1 so that we highlight at
    // least the first character.
    Scanner::Location loc(declaration->proxy()->position(),
                          var_end_pos != kNoSourcePosition
                              ? var_end_pos
                              : declaration->proxy()->position() + 1);
    if (declaration_kind == DeclarationDescriptor::PARAMETER) {
      ReportMessageAt(loc, MessageTemplate::kParamDupe);
    } else {
      ReportMessageAt(loc, MessageTemplate::kVarRedeclaration,
                      declaration->proxy()->raw_name());
    }
    return nullptr;
  }
  if (sloppy_mode_block_scope_function_redefinition) {
    ++use_counts_[v8::Isolate::kSloppyModeBlockScopedFunctionRedefinition];
  }
  return variable;
}

Block* Parser::BuildInitializationBlock(
    DeclarationParsingResult* parsing_result,
    ZoneList<const AstRawString*>* names, bool* ok) {
  Block* result = factory()->NewBlock(1, true);
  for (auto declaration : parsing_result->declarations) {
    DeclareAndInitializeVariables(result, &(parsing_result->descriptor),
                                  &declaration, names, CHECK_OK);
  }
  return result;
}

Statement* Parser::DeclareFunction(const AstRawString* variable_name,
                                   FunctionLiteral* function, VariableMode mode,
                                   int pos, bool is_sloppy_block_function,
                                   ZoneList<const AstRawString*>* names,
                                   bool* ok) {
  VariableProxy* proxy =
      factory()->NewVariableProxy(variable_name, NORMAL_VARIABLE, pos);
  Declaration* declaration =
      factory()->NewFunctionDeclaration(proxy, function, pos);
  Declare(declaration, DeclarationDescriptor::NORMAL, mode, kCreatedInitialized,
          CHECK_OK);
  if (names) names->Add(variable_name, zone());
  if (is_sloppy_block_function) {
    SloppyBlockFunctionStatement* statement =
        factory()->NewSloppyBlockFunctionStatement();
    GetDeclarationScope()->DeclareSloppyBlockFunction(variable_name, scope(),
                                                      statement);
    return statement;
  }
  return factory()->NewEmptyStatement(kNoSourcePosition);
}

Statement* Parser::DeclareClass(const AstRawString* variable_name,
                                Expression* value,
                                ZoneList<const AstRawString*>* names,
                                int class_token_pos, int end_pos, bool* ok) {
  Declaration* decl =
      DeclareVariable(variable_name, LET, class_token_pos, CHECK_OK);
  decl->proxy()->var()->set_initializer_position(end_pos);
  if (names) names->Add(variable_name, zone());

  Assignment* assignment = factory()->NewAssignment(Token::INIT, decl->proxy(),
                                                    value, class_token_pos);
  return IgnoreCompletion(
      factory()->NewExpressionStatement(assignment, kNoSourcePosition));
}

Statement* Parser::DeclareNative(const AstRawString* name, int pos, bool* ok) {
  // Make sure that the function containing the native declaration
  // isn't lazily compiled. The extension structures are only
  // accessible while parsing the first time not when reparsing
  // because of lazy compilation.
  GetClosureScope()->ForceEagerCompilation();

  // TODO(1240846): It's weird that native function declarations are
  // introduced dynamically when we meet their declarations, whereas
  // other functions are set up when entering the surrounding scope.
  Declaration* decl = DeclareVariable(name, VAR, pos, CHECK_OK);
  NativeFunctionLiteral* lit =
      factory()->NewNativeFunctionLiteral(name, extension_, kNoSourcePosition);
  return factory()->NewExpressionStatement(
      factory()->NewAssignment(Token::INIT, decl->proxy(), lit,
                               kNoSourcePosition),
      pos);
}

ZoneList<const AstRawString*>* Parser::DeclareLabel(
    ZoneList<const AstRawString*>* labels, VariableProxy* var, bool* ok) {
  DCHECK(IsIdentifier(var));
  const AstRawString* label = var->raw_name();
  // TODO(1240780): We don't check for redeclaration of labels
  // during preparsing since keeping track of the set of active
  // labels requires nontrivial changes to the way scopes are
  // structured.  However, these are probably changes we want to
  // make later anyway so we should go back and fix this then.
  if (ContainsLabel(labels, label) || TargetStackContainsLabel(label)) {
    ReportMessage(MessageTemplate::kLabelRedeclaration, label);
    *ok = false;
    return nullptr;
  }
  if (labels == nullptr) {
    labels = new (zone()) ZoneList<const AstRawString*>(1, zone());
  }
  labels->Add(label, zone());
  // Remove the "ghost" variable that turned out to be a label
  // from the top scope. This way, we don't try to resolve it
  // during the scope processing.
  scope()->RemoveUnresolved(var);
  return labels;
}

bool Parser::ContainsLabel(ZoneList<const AstRawString*>* labels,
                           const AstRawString* label) {
  DCHECK_NOT_NULL(label);
  if (labels != nullptr) {
    for (int i = labels->length(); i-- > 0;) {
      if (labels->at(i) == label) return true;
    }
  }
  return false;
}

Block* Parser::IgnoreCompletion(Statement* statement) {
  Block* block = factory()->NewBlock(1, true);
  block->statements()->Add(statement, zone());
  return block;
}

Expression* Parser::RewriteReturn(Expression* return_value, int pos) {
  if (IsDerivedConstructor(function_state_->kind())) {
    // For subclass constructors we need to return this in case of undefined;
    // other primitive values trigger an exception in the ConstructStub.
    //
    //   return expr;
    //
    // Is rewritten as:
    //
    //   return (temp = expr) === undefined ? this : temp;

    // temp = expr
    Variable* temp = NewTemporary(ast_value_factory()->empty_string());
    Assignment* assign = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(temp), return_value, pos);

    // temp === undefined
    Expression* is_undefined = factory()->NewCompareOperation(
        Token::EQ_STRICT, assign,
        factory()->NewUndefinedLiteral(kNoSourcePosition), pos);

    // is_undefined ? this : temp
    return_value =
        factory()->NewConditional(is_undefined, ThisExpression(pos),
                                  factory()->NewVariableProxy(temp), pos);
  }
  return return_value;
}

Expression* Parser::RewriteDoExpression(Block* body, int pos, bool* ok) {
  Variable* result = NewTemporary(ast_value_factory()->dot_result_string());
  DoExpression* expr = factory()->NewDoExpression(body, result, pos);
  if (!Rewriter::Rewrite(this, GetClosureScope(), expr, ast_value_factory())) {
    *ok = false;
    return nullptr;
  }
  return expr;
}

Statement* Parser::RewriteSwitchStatement(SwitchStatement* switch_statement,
                                          Scope* scope) {
  // In order to get the CaseClauses to execute in their own lexical scope,
  // but without requiring downstream code to have special scope handling
  // code for switch statements, desugar into blocks as follows:
  // {  // To group the statements--harmless to evaluate Expression in scope
  //   .tag_variable = Expression;
  //   {  // To give CaseClauses a scope
  //     switch (.tag_variable) { CaseClause* }
  //   }
  // }
  DCHECK_NOT_NULL(scope);
  DCHECK(scope->is_block_scope());
  DCHECK_GE(switch_statement->position(), scope->start_position());
  DCHECK_LT(switch_statement->position(), scope->end_position());

  Block* switch_block = factory()->NewBlock(2, false);

  Expression* tag = switch_statement->tag();
  Variable* tag_variable =
      NewTemporary(ast_value_factory()->dot_switch_tag_string());
  Assignment* tag_assign = factory()->NewAssignment(
      Token::ASSIGN, factory()->NewVariableProxy(tag_variable), tag,
      tag->position());
  // Wrap with IgnoreCompletion so the tag isn't returned as the completion
  // value, in case the switch statements don't have a value.
  Statement* tag_statement = IgnoreCompletion(
      factory()->NewExpressionStatement(tag_assign, kNoSourcePosition));
  switch_block->statements()->Add(tag_statement, zone());

  switch_statement->set_tag(factory()->NewVariableProxy(tag_variable));
  Block* cases_block = factory()->NewBlock(1, false);
  cases_block->statements()->Add(switch_statement, zone());
  cases_block->set_scope(scope);
  switch_block->statements()->Add(cases_block, zone());
  return switch_block;
}

void Parser::RewriteCatchPattern(CatchInfo* catch_info, bool* ok) {
  if (catch_info->name == nullptr) {
    DCHECK_NOT_NULL(catch_info->pattern);
    catch_info->name = ast_value_factory()->dot_catch_string();
  }
  Variable* catch_variable =
      catch_info->scope->DeclareLocal(catch_info->name, VAR);
  if (catch_info->pattern != nullptr) {
    DeclarationDescriptor descriptor;
    descriptor.declaration_kind = DeclarationDescriptor::NORMAL;
    descriptor.scope = scope();
    descriptor.mode = LET;
    descriptor.declaration_pos = catch_info->pattern->position();
    descriptor.initialization_pos = catch_info->pattern->position();

    // Initializer position for variables declared by the pattern.
    const int initializer_position = position();

    DeclarationParsingResult::Declaration decl(
        catch_info->pattern, initializer_position,
        factory()->NewVariableProxy(catch_variable));

    catch_info->init_block = factory()->NewBlock(8, true);
    DeclareAndInitializeVariables(catch_info->init_block, &descriptor, &decl,
                                  &catch_info->bound_names, ok);
  } else {
    catch_info->bound_names.Add(catch_info->name, zone());
  }
}

void Parser::ValidateCatchBlock(const CatchInfo& catch_info, bool* ok) {
  // Check for `catch(e) { let e; }` and similar errors.
  Scope* inner_block_scope = catch_info.inner_block->scope();
  if (inner_block_scope != nullptr) {
    Declaration* decl = inner_block_scope->CheckLexDeclarationsConflictingWith(
        catch_info.bound_names);
    if (decl != nullptr) {
      const AstRawString* name = decl->proxy()->raw_name();
      int position = decl->proxy()->position();
      Scanner::Location location =
          position == kNoSourcePosition
              ? Scanner::Location::invalid()
              : Scanner::Location(position, position + 1);
      ReportMessageAt(location, MessageTemplate::kVarRedeclaration, name);
      *ok = false;
    }
  }
}

Statement* Parser::RewriteTryStatement(Block* try_block, Block* catch_block,
                                       const SourceRange& catch_range,
                                       Block* finally_block,
                                       const SourceRange& finally_range,
                                       const CatchInfo& catch_info, int pos) {
  // Simplify the AST nodes by converting:
  //   'try B0 catch B1 finally B2'
  // to:
  //   'try { try B0 catch B1 } finally B2'

  if (catch_block != nullptr && finally_block != nullptr) {
    // If we have both, create an inner try/catch.
    TryCatchStatement* statement;
    statement = factory()->NewTryCatchStatement(try_block, catch_info.scope,
                                                catch_block, kNoSourcePosition);
    RecordTryCatchStatementSourceRange(statement, catch_range);

    try_block = factory()->NewBlock(1, false);
    try_block->statements()->Add(statement, zone());
    catch_block = nullptr;  // Clear to indicate it's been handled.
  }

  if (catch_block != nullptr) {
    DCHECK_NULL(finally_block);
    TryCatchStatement* stmt = factory()->NewTryCatchStatement(
        try_block, catch_info.scope, catch_block, pos);
    RecordTryCatchStatementSourceRange(stmt, catch_range);
    return stmt;
  } else {
    DCHECK_NOT_NULL(finally_block);
    TryFinallyStatement* stmt =
        factory()->NewTryFinallyStatement(try_block, finally_block, pos);
    RecordTryFinallyStatementSourceRange(stmt, finally_range);
    return stmt;
  }
}

void Parser::ParseAndRewriteGeneratorFunctionBody(int pos, FunctionKind kind,
                                                  ZoneList<Statement*>* body,
                                                  bool* ok) {
  // For ES6 Generators, we just prepend the initial yield.
  Expression* initial_yield = BuildInitialYield(pos, kind);
  body->Add(factory()->NewExpressionStatement(initial_yield, kNoSourcePosition),
            zone());
  ParseStatementList(body, Token::RBRACE, ok);
}

void Parser::ParseAndRewriteAsyncGeneratorFunctionBody(
    int pos, FunctionKind kind, ZoneList<Statement*>* body, bool* ok) {
  // For ES2017 Async Generators, we produce:
  //
  // try {
  //   InitialYield;
  //   ...body...;
  //   return undefined; // See comment below
  // } catch (.catch) {
  //   %AsyncGeneratorReject(generator, .catch);
  // } finally {
  //   %_GeneratorClose(generator);
  // }
  //
  // - InitialYield yields the actual generator object.
  // - Any return statement inside the body will have its argument wrapped
  //   in an iterator result object with a "done" property set to `true`.
  // - If the generator terminates for whatever reason, we must close it.
  //   Hence the finally clause.
  // - BytecodeGenerator performs special handling for ReturnStatements in
  //   async generator functions, resolving the appropriate Promise with an
  //   "done" iterator result object containing a Promise-unwrapped value.
  DCHECK(IsAsyncGeneratorFunction(kind));

  Block* try_block = factory()->NewBlock(3, false);
  Expression* initial_yield = BuildInitialYield(pos, kind);
  try_block->statements()->Add(
      factory()->NewExpressionStatement(initial_yield, kNoSourcePosition),
      zone());
  ParseStatementList(try_block->statements(), Token::RBRACE, ok);
  if (!*ok) return;

  // Don't create iterator result for async generators, as the resume methods
  // will create it.
  // TODO(leszeks): This will create another suspend point, which is unnecessary
  // if there is already an unconditional return in the body.
  Statement* final_return = BuildReturnStatement(
      factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);
  try_block->statements()->Add(final_return, zone());

  // For AsyncGenerators, a top-level catch block will reject the Promise.
  Scope* catch_scope = NewHiddenCatchScope();

  ZoneList<Expression*>* reject_args =
      new (zone()) ZoneList<Expression*>(2, zone());
  reject_args->Add(factory()->NewVariableProxy(
                       function_state_->scope()->generator_object_var()),
                   zone());
  reject_args->Add(factory()->NewVariableProxy(catch_scope->catch_variable()),
                   zone());

  Expression* reject_call = factory()->NewCallRuntime(
      Runtime::kInlineAsyncGeneratorReject, reject_args, kNoSourcePosition);
  Block* catch_block = IgnoreCompletion(
      factory()->NewReturnStatement(reject_call, kNoSourcePosition));

  TryStatement* try_catch = factory()->NewTryCatchStatementForAsyncAwait(
      try_block, catch_scope, catch_block, kNoSourcePosition);

  try_block = factory()->NewBlock(1, false);
  try_block->statements()->Add(try_catch, zone());

  Block* finally_block = factory()->NewBlock(1, false);
  ZoneList<Expression*>* close_args =
      new (zone()) ZoneList<Expression*>(1, zone());
  VariableProxy* call_proxy = factory()->NewVariableProxy(
      function_state_->scope()->generator_object_var());
  close_args->Add(call_proxy, zone());
  Expression* close_call = factory()->NewCallRuntime(
      Runtime::kInlineGeneratorClose, close_args, kNoSourcePosition);
  finally_block->statements()->Add(
      factory()->NewExpressionStatement(close_call, kNoSourcePosition), zone());

  body->Add(factory()->NewTryFinallyStatement(try_block, finally_block,
                                              kNoSourcePosition),
            zone());
}

void Parser::DeclareFunctionNameVar(const AstRawString* function_name,
                                    FunctionLiteral::FunctionType function_type,
                                    DeclarationScope* function_scope) {
  if (function_type == FunctionLiteral::kNamedExpression &&
      function_scope->LookupLocal(function_name) == nullptr) {
    DCHECK_EQ(function_scope, scope());
    function_scope->DeclareFunctionVar(function_name);
  }
}

// [if (IteratorType == kNormal)]
//     !%_IsJSReceiver(result = iterator.next()) &&
//         %ThrowIteratorResultNotAnObject(result)
// [else if (IteratorType == kAsync)]
//     !%_IsJSReceiver(result = Await(iterator.next())) &&
//         %ThrowIteratorResultNotAnObject(result)
// [endif]
Expression* Parser::BuildIteratorNextResult(VariableProxy* iterator,
                                            VariableProxy* next,
                                            Variable* result, IteratorType type,
                                            int pos) {
  Expression* next_property = factory()->NewResolvedProperty(iterator, next);
  ZoneList<Expression*>* next_arguments =
      new (zone()) ZoneList<Expression*>(0, zone());
  Expression* next_call =
      factory()->NewCall(next_property, next_arguments, kNoSourcePosition);
  if (type == IteratorType::kAsync) {
    function_state_->AddSuspend();
    next_call = factory()->NewAwait(next_call, pos);
  }
  Expression* result_proxy = factory()->NewVariableProxy(result);
  Expression* left =
      factory()->NewAssignment(Token::ASSIGN, result_proxy, next_call, pos);

  // %_IsJSReceiver(...)
  ZoneList<Expression*>* is_spec_object_args =
      new (zone()) ZoneList<Expression*>(1, zone());
  is_spec_object_args->Add(left, zone());
  Expression* is_spec_object_call = factory()->NewCallRuntime(
      Runtime::kInlineIsJSReceiver, is_spec_object_args, pos);

  // %ThrowIteratorResultNotAnObject(result)
  Expression* result_proxy_again = factory()->NewVariableProxy(result);
  ZoneList<Expression*>* throw_arguments =
      new (zone()) ZoneList<Expression*>(1, zone());
  throw_arguments->Add(result_proxy_again, zone());
  Expression* throw_call = factory()->NewCallRuntime(
      Runtime::kThrowIteratorResultNotAnObject, throw_arguments, pos);

  return factory()->NewBinaryOperation(
      Token::AND,
      factory()->NewUnaryOperation(Token::NOT, is_spec_object_call, pos),
      throw_call, pos);
}

Statement* Parser::InitializeForEachStatement(ForEachStatement* stmt,
                                              Expression* each,
                                              Expression* subject,
                                              Statement* body) {
  ForOfStatement* for_of = stmt->AsForOfStatement();
  if (for_of != nullptr) {
    const bool finalize = true;
    return InitializeForOfStatement(for_of, each, subject, body, finalize,
                                    IteratorType::kNormal, each->position());
  } else {
    if (each->IsArrayLiteral() || each->IsObjectLiteral()) {
      Variable* temp = NewTemporary(ast_value_factory()->empty_string());
      VariableProxy* temp_proxy = factory()->NewVariableProxy(temp);
      Expression* assign_each =
          RewriteDestructuringAssignment(factory()->NewAssignment(
              Token::ASSIGN, each, temp_proxy, kNoSourcePosition));
      auto block = factory()->NewBlock(2, false);
      block->statements()->Add(
          factory()->NewExpressionStatement(assign_each, kNoSourcePosition),
          zone());
      block->statements()->Add(body, zone());
      body = block;
      each = factory()->NewVariableProxy(temp);
    }
    MarkExpressionAsAssigned(each);
    stmt->AsForInStatement()->Initialize(each, subject, body);
  }
  return stmt;
}

// Special case for legacy for
//
//    for (var x = initializer in enumerable) body
//
// An initialization block of the form
//
//    {
//      x = initializer;
//    }
//
// is returned in this case.  It has reserved space for two statements,
// so that (later on during parsing), the equivalent of
//
//   for (x in enumerable) body
//
// is added as a second statement to it.
Block* Parser::RewriteForVarInLegacy(const ForInfo& for_info) {
  const DeclarationParsingResult::Declaration& decl =
      for_info.parsing_result.declarations[0];
  if (!IsLexicalVariableMode(for_info.parsing_result.descriptor.mode) &&
      decl.pattern->IsVariableProxy() && decl.initializer != nullptr) {
    ++use_counts_[v8::Isolate::kForInInitializer];
    const AstRawString* name = decl.pattern->AsVariableProxy()->raw_name();
    VariableProxy* single_var = NewUnresolved(name);
    Block* init_block = factory()->NewBlock(2, true);
    init_block->statements()->Add(
        factory()->NewExpressionStatement(
            factory()->NewAssignment(Token::ASSIGN, single_var,
                                     decl.initializer, kNoSourcePosition),
            kNoSourcePosition),
        zone());
    return init_block;
  }
  return nullptr;
}

// Rewrite a for-in/of statement of the form
//
//   for (let/const/var x in/of e) b
//
// into
//
//   {
//     var temp;
//     for (temp in/of e) {
//       let/const/var x = temp;
//       b;
//     }
//     let x;  // for TDZ
//   }
void Parser::DesugarBindingInForEachStatement(ForInfo* for_info,
                                              Block** body_block,
                                              Expression** each_variable,
                                              bool* ok) {
  DCHECK_EQ(1, for_info->parsing_result.declarations.size());
  DeclarationParsingResult::Declaration& decl =
      for_info->parsing_result.declarations[0];
  Variable* temp = NewTemporary(ast_value_factory()->dot_for_string());
  auto each_initialization_block = factory()->NewBlock(1, true);
  {
    auto descriptor = for_info->parsing_result.descriptor;
    descriptor.declaration_pos = kNoSourcePosition;
    descriptor.initialization_pos = kNoSourcePosition;
    descriptor.scope = scope();
    decl.initializer = factory()->NewVariableProxy(temp);

    bool is_for_var_of =
        for_info->mode == ForEachStatement::ITERATE &&
        for_info->parsing_result.descriptor.mode == VariableMode::VAR;
    bool collect_names =
        IsLexicalVariableMode(for_info->parsing_result.descriptor.mode) ||
        is_for_var_of;

    DeclareAndInitializeVariables(
        each_initialization_block, &descriptor, &decl,
        collect_names ? &for_info->bound_names : nullptr, CHECK_OK_VOID);

    // Annex B.3.5 prohibits the form
    // `try {} catch(e) { for (var e of {}); }`
    // So if we are parsing a statement like `for (var ... of ...)`
    // we need to walk up the scope chain and look for catch scopes
    // which have a simple binding, then compare their binding against
    // all of the names declared in the init of the for-of we're
    // parsing.
    if (is_for_var_of) {
      Scope* catch_scope = scope();
      while (catch_scope != nullptr && !catch_scope->is_declaration_scope()) {
        if (catch_scope->is_catch_scope()) {
          auto name = catch_scope->catch_variable()->raw_name();
          // If it's a simple binding and the name is declared in the for loop.
          if (name != ast_value_factory()->dot_catch_string() &&
              for_info->bound_names.Contains(name)) {
            ReportMessageAt(for_info->parsing_result.bindings_loc,
                            MessageTemplate::kVarRedeclaration, name);
            *ok = false;
            return;
          }
        }
        catch_scope = catch_scope->outer_scope();
      }
    }
  }

  *body_block = factory()->NewBlock(3, false);
  (*body_block)->statements()->Add(each_initialization_block, zone());
  *each_variable = factory()->NewVariableProxy(temp, for_info->position);
}

// Create a TDZ for any lexically-bound names in for in/of statements.
Block* Parser::CreateForEachStatementTDZ(Block* init_block,
                                         const ForInfo& for_info, bool* ok) {
  if (IsLexicalVariableMode(for_info.parsing_result.descriptor.mode)) {
    DCHECK_NULL(init_block);

    init_block = factory()->NewBlock(1, false);

    for (int i = 0; i < for_info.bound_names.length(); ++i) {
      // TODO(adamk): This needs to be some sort of special
      // INTERNAL variable that's invisible to the debugger
      // but visible to everything else.
      Declaration* tdz_decl = DeclareVariable(for_info.bound_names[i], LET,
                                              kNoSourcePosition, CHECK_OK);
      tdz_decl->proxy()->var()->set_initializer_position(position());
    }
  }
  return init_block;
}

Statement* Parser::InitializeForOfStatement(
    ForOfStatement* for_of, Expression* each, Expression* iterable,
    Statement* body, bool finalize, IteratorType type, int next_result_pos) {
  // Create the auxiliary expressions needed for iterating over the iterable,
  // and initialize the given ForOfStatement with them.
  // If finalize is true, also instrument the loop with code that performs the
  // proper ES6 iterator finalization.  In that case, the result is not
  // immediately a ForOfStatement.
  const int nopos = kNoSourcePosition;
  auto avfactory = ast_value_factory();

  Variable* iterator = NewTemporary(avfactory->dot_iterator_string());
  Variable* next = NewTemporary(avfactory->empty_string());
  Variable* result = NewTemporary(avfactory->dot_result_string());
  Variable* completion = NewTemporary(avfactory->empty_string());

  // iterator = GetIterator(iterable, type)
  Expression* assign_iterator;
  {
    assign_iterator = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(iterator),
        factory()->NewGetIterator(iterable, type, iterable->position()),
        iterable->position());
  }

  Expression* assign_next;
  {
    assign_next = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(next),
        factory()->NewProperty(factory()->NewVariableProxy(iterator),
                               factory()->NewStringLiteral(
                                   avfactory->next_string(), kNoSourcePosition),
                               kNoSourcePosition),
        kNoSourcePosition);
  }

  // [if (IteratorType == kNormal)]
  //     !%_IsJSReceiver(result = iterator.next()) &&
  //         %ThrowIteratorResultNotAnObject(result)
  // [else if (IteratorType == kAsync)]
  //     !%_IsJSReceiver(result = Await(iterator.next())) &&
  //         %ThrowIteratorResultNotAnObject(result)
  // [endif]
  Expression* next_result;
  {
    VariableProxy* iterator_proxy = factory()->NewVariableProxy(iterator);
    VariableProxy* next_proxy = factory()->NewVariableProxy(next);
    next_result = BuildIteratorNextResult(iterator_proxy, next_proxy, result,
                                          type, next_result_pos);
  }

  // result.done
  Expression* result_done;
  {
    Expression* done_literal = factory()->NewStringLiteral(
        ast_value_factory()->done_string(), kNoSourcePosition);
    Expression* result_proxy = factory()->NewVariableProxy(result);
    result_done =
        factory()->NewProperty(result_proxy, done_literal, kNoSourcePosition);
  }

  // result.value
  Expression* result_value;
  {
    Expression* value_literal =
        factory()->NewStringLiteral(avfactory->value_string(), nopos);
    Expression* result_proxy = factory()->NewVariableProxy(result);
    result_value = factory()->NewProperty(result_proxy, value_literal, nopos);
  }

  // {{tmp = #result_value, completion = kAbruptCompletion, tmp}}
  // Expression* result_value (gets overwritten)
  if (finalize) {
    Variable* tmp = NewTemporary(avfactory->empty_string());
    Expression* save_result = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(tmp), result_value, nopos);

    Expression* set_completion_abrupt = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(completion),
        factory()->NewSmiLiteral(Parser::kAbruptCompletion, nopos), nopos);

    result_value = factory()->NewBinaryOperation(Token::COMMA, save_result,
                                                 set_completion_abrupt, nopos);
    result_value = factory()->NewBinaryOperation(
        Token::COMMA, result_value, factory()->NewVariableProxy(tmp), nopos);
  }

  // each = #result_value;
  Expression* assign_each;
  {
    assign_each =
        factory()->NewAssignment(Token::ASSIGN, each, result_value, nopos);
    if (each->IsArrayLiteral() || each->IsObjectLiteral()) {
      assign_each = RewriteDestructuringAssignment(assign_each->AsAssignment());
    }
  }

  // {{completion = kNormalCompletion;}}
  Statement* set_completion_normal;
  if (finalize) {
    Expression* proxy = factory()->NewVariableProxy(completion);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, proxy,
        factory()->NewSmiLiteral(Parser::kNormalCompletion, nopos), nopos);

    set_completion_normal =
        IgnoreCompletion(factory()->NewExpressionStatement(assignment, nopos));
  }

  // { #loop-body; #set_completion_normal }
  // Statement* body (gets overwritten)
  if (finalize) {
    Block* block = factory()->NewBlock(2, false);
    block->statements()->Add(body, zone());
    block->statements()->Add(set_completion_normal, zone());
    body = block;
  }

  for_of->Initialize(body, iterator, assign_iterator, assign_next, next_result,
                     result_done, assign_each);
  return finalize ? FinalizeForOfStatement(for_of, completion, type, nopos)
                  : for_of;
}

Statement* Parser::DesugarLexicalBindingsInForStatement(
    ForStatement* loop, Statement* init, Expression* cond, Statement* next,
    Statement* body, Scope* inner_scope, const ForInfo& for_info, bool* ok) {
  // ES6 13.7.4.8 specifies that on each loop iteration the let variables are
  // copied into a new environment.  Moreover, the "next" statement must be
  // evaluated not in the environment of the just completed iteration but in
  // that of the upcoming one.  We achieve this with the following desugaring.
  // Extra care is needed to preserve the completion value of the original loop.
  //
  // We are given a for statement of the form
  //
  //  labels: for (let/const x = i; cond; next) body
  //
  // and rewrite it as follows.  Here we write {{ ... }} for init-blocks, ie.,
  // blocks whose ignore_completion_value_ flag is set.
  //
  //  {
  //    let/const x = i;
  //    temp_x = x;
  //    first = 1;
  //    undefined;
  //    outer: for (;;) {
  //      let/const x = temp_x;
  //      {{ if (first == 1) {
  //           first = 0;
  //         } else {
  //           next;
  //         }
  //         flag = 1;
  //         if (!cond) break;
  //      }}
  //      labels: for (; flag == 1; flag = 0, temp_x = x) {
  //        body
  //      }
  //      {{ if (flag == 1)  // Body used break.
  //           break;
  //      }}
  //    }
  //  }

  DCHECK_GT(for_info.bound_names.length(), 0);
  ZoneList<Variable*> temps(for_info.bound_names.length(), zone());

  Block* outer_block =
      factory()->NewBlock(for_info.bound_names.length() + 4, false);

  // Add statement: let/const x = i.
  outer_block->statements()->Add(init, zone());

  const AstRawString* temp_name = ast_value_factory()->dot_for_string();

  // For each lexical variable x:
  //   make statement: temp_x = x.
  for (int i = 0; i < for_info.bound_names.length(); i++) {
    VariableProxy* proxy = NewUnresolved(for_info.bound_names[i]);
    Variable* temp = NewTemporary(temp_name);
    VariableProxy* temp_proxy = factory()->NewVariableProxy(temp);
    Assignment* assignment = factory()->NewAssignment(Token::ASSIGN, temp_proxy,
                                                      proxy, kNoSourcePosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, kNoSourcePosition);
    outer_block->statements()->Add(assignment_statement, zone());
    temps.Add(temp, zone());
  }

  Variable* first = nullptr;
  // Make statement: first = 1.
  if (next) {
    first = NewTemporary(temp_name);
    VariableProxy* first_proxy = factory()->NewVariableProxy(first);
    Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, first_proxy, const1, kNoSourcePosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, kNoSourcePosition);
    outer_block->statements()->Add(assignment_statement, zone());
  }

  // make statement: undefined;
  outer_block->statements()->Add(
      factory()->NewExpressionStatement(
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition),
      zone());

  // Make statement: outer: for (;;)
  // Note that we don't actually create the label, or set this loop up as an
  // explicit break target, instead handing it directly to those nodes that
  // need to know about it. This should be safe because we don't run any code
  // in this function that looks up break targets.
  ForStatement* outer_loop =
      factory()->NewForStatement(nullptr, kNoSourcePosition);
  outer_block->statements()->Add(outer_loop, zone());
  outer_block->set_scope(scope());

  Block* inner_block = factory()->NewBlock(3, false);
  {
    BlockState block_state(&scope_, inner_scope);

    Block* ignore_completion_block =
        factory()->NewBlock(for_info.bound_names.length() + 3, true);
    ZoneList<Variable*> inner_vars(for_info.bound_names.length(), zone());
    // For each let variable x:
    //    make statement: let/const x = temp_x.
    for (int i = 0; i < for_info.bound_names.length(); i++) {
      Declaration* decl = DeclareVariable(
          for_info.bound_names[i], for_info.parsing_result.descriptor.mode,
          kNoSourcePosition, CHECK_OK);
      inner_vars.Add(decl->proxy()->var(), zone());
      VariableProxy* temp_proxy = factory()->NewVariableProxy(temps.at(i));
      Assignment* assignment = factory()->NewAssignment(
          Token::INIT, decl->proxy(), temp_proxy, kNoSourcePosition);
      Statement* assignment_statement =
          factory()->NewExpressionStatement(assignment, kNoSourcePosition);
      int declaration_pos = for_info.parsing_result.descriptor.declaration_pos;
      DCHECK_NE(declaration_pos, kNoSourcePosition);
      decl->proxy()->var()->set_initializer_position(declaration_pos);
      ignore_completion_block->statements()->Add(assignment_statement, zone());
    }

    // Make statement: if (first == 1) { first = 0; } else { next; }
    if (next) {
      DCHECK(first);
      Expression* compare = nullptr;
      // Make compare expression: first == 1.
      {
        Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
        VariableProxy* first_proxy = factory()->NewVariableProxy(first);
        compare = factory()->NewCompareOperation(Token::EQ, first_proxy, const1,
                                                 kNoSourcePosition);
      }
      Statement* clear_first = nullptr;
      // Make statement: first = 0.
      {
        VariableProxy* first_proxy = factory()->NewVariableProxy(first);
        Expression* const0 = factory()->NewSmiLiteral(0, kNoSourcePosition);
        Assignment* assignment = factory()->NewAssignment(
            Token::ASSIGN, first_proxy, const0, kNoSourcePosition);
        clear_first =
            factory()->NewExpressionStatement(assignment, kNoSourcePosition);
      }
      Statement* clear_first_or_next = factory()->NewIfStatement(
          compare, clear_first, next, kNoSourcePosition);
      ignore_completion_block->statements()->Add(clear_first_or_next, zone());
    }

    Variable* flag = NewTemporary(temp_name);
    // Make statement: flag = 1.
    {
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
      Assignment* assignment = factory()->NewAssignment(
          Token::ASSIGN, flag_proxy, const1, kNoSourcePosition);
      Statement* assignment_statement =
          factory()->NewExpressionStatement(assignment, kNoSourcePosition);
      ignore_completion_block->statements()->Add(assignment_statement, zone());
    }

    // Make statement: if (!cond) break.
    if (cond) {
      Statement* stop =
          factory()->NewBreakStatement(outer_loop, kNoSourcePosition);
      Statement* noop = factory()->NewEmptyStatement(kNoSourcePosition);
      ignore_completion_block->statements()->Add(
          factory()->NewIfStatement(cond, noop, stop, cond->position()),
          zone());
    }

    inner_block->statements()->Add(ignore_completion_block, zone());
    // Make cond expression for main loop: flag == 1.
    Expression* flag_cond = nullptr;
    {
      Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      flag_cond = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                                 kNoSourcePosition);
    }

    // Create chain of expressions "flag = 0, temp_x = x, ..."
    Statement* compound_next_statement = nullptr;
    {
      Expression* compound_next = nullptr;
      // Make expression: flag = 0.
      {
        VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
        Expression* const0 = factory()->NewSmiLiteral(0, kNoSourcePosition);
        compound_next = factory()->NewAssignment(Token::ASSIGN, flag_proxy,
                                                 const0, kNoSourcePosition);
      }

      // Make the comma-separated list of temp_x = x assignments.
      int inner_var_proxy_pos = scanner()->location().beg_pos;
      for (int i = 0; i < for_info.bound_names.length(); i++) {
        VariableProxy* temp_proxy = factory()->NewVariableProxy(temps.at(i));
        VariableProxy* proxy =
            factory()->NewVariableProxy(inner_vars.at(i), inner_var_proxy_pos);
        Assignment* assignment = factory()->NewAssignment(
            Token::ASSIGN, temp_proxy, proxy, kNoSourcePosition);
        compound_next = factory()->NewBinaryOperation(
            Token::COMMA, compound_next, assignment, kNoSourcePosition);
      }

      compound_next_statement =
          factory()->NewExpressionStatement(compound_next, kNoSourcePosition);
    }

    // Make statement: labels: for (; flag == 1; flag = 0, temp_x = x)
    // Note that we re-use the original loop node, which retains its labels
    // and ensures that any break or continue statements in body point to
    // the right place.
    loop->Initialize(nullptr, flag_cond, compound_next_statement, body);
    inner_block->statements()->Add(loop, zone());

    // Make statement: {{if (flag == 1) break;}}
    {
      Expression* compare = nullptr;
      // Make compare expresion: flag == 1.
      {
        Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
        VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
        compare = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                                 kNoSourcePosition);
      }
      Statement* stop =
          factory()->NewBreakStatement(outer_loop, kNoSourcePosition);
      Statement* empty = factory()->NewEmptyStatement(kNoSourcePosition);
      Statement* if_flag_break =
          factory()->NewIfStatement(compare, stop, empty, kNoSourcePosition);
      inner_block->statements()->Add(IgnoreCompletion(if_flag_break), zone());
    }

    inner_block->set_scope(inner_scope);
  }

  outer_loop->Initialize(nullptr, nullptr, nullptr, inner_block);

  return outer_block;
}

void Parser::AddArrowFunctionFormalParameters(
    ParserFormalParameters* parameters, Expression* expr, int end_pos,
    bool* ok) {
  // ArrowFunctionFormals ::
  //    Nary(Token::COMMA, VariableProxy*, Tail)
  //    Binary(Token::COMMA, NonTailArrowFunctionFormals, Tail)
  //    Tail
  // NonTailArrowFunctionFormals ::
  //    Binary(Token::COMMA, NonTailArrowFunctionFormals, VariableProxy)
  //    VariableProxy
  // Tail ::
  //    VariableProxy
  //    Spread(VariableProxy)
  //
  // We need to visit the parameters in left-to-right order
  //

  // For the Nary case, we simply visit the parameters in a loop.
  if (expr->IsNaryOperation()) {
    NaryOperation* nary = expr->AsNaryOperation();
    // The classifier has already run, so we know that the expression is a valid
    // arrow function formals production.
    DCHECK_EQ(nary->op(), Token::COMMA);
    // Each op position is the end position of the *previous* expr, with the
    // second (i.e. first "subsequent") op position being the end position of
    // the first child expression.
    Expression* next = nary->first();
    for (size_t i = 0; i < nary->subsequent_length(); ++i) {
      AddArrowFunctionFormalParameters(
          parameters, next, nary->subsequent_op_position(i), CHECK_OK_VOID);
      next = nary->subsequent(i);
    }
    AddArrowFunctionFormalParameters(parameters, next, end_pos, CHECK_OK_VOID);
    return;
  }

  // For the binary case, we recurse on the left-hand side of binary comma
  // expressions.
  if (expr->IsBinaryOperation()) {
    BinaryOperation* binop = expr->AsBinaryOperation();
    // The classifier has already run, so we know that the expression is a valid
    // arrow function formals production.
    DCHECK_EQ(binop->op(), Token::COMMA);
    Expression* left = binop->left();
    Expression* right = binop->right();
    int comma_pos = binop->position();
    AddArrowFunctionFormalParameters(parameters, left, comma_pos,
                                     CHECK_OK_VOID);
    // LHS of comma expression should be unparenthesized.
    expr = right;
  }

  // Only the right-most expression may be a rest parameter.
  DCHECK(!parameters->has_rest);

  bool is_rest = expr->IsSpread();
  if (is_rest) {
    expr = expr->AsSpread()->expression();
    parameters->has_rest = true;
  }
  DCHECK_IMPLIES(parameters->is_simple, !is_rest);
  DCHECK_IMPLIES(parameters->is_simple, expr->IsVariableProxy());

  Expression* initializer = nullptr;
  if (expr->IsAssignment()) {
    if (expr->IsRewritableExpression()) {
      // This expression was parsed as a possible destructuring assignment.
      // Mark it as already-rewritten to avoid an unnecessary visit later.
      expr->AsRewritableExpression()->set_rewritten();
    }
    Assignment* assignment = expr->AsAssignment();
    DCHECK(!assignment->IsCompoundAssignment());
    initializer = assignment->value();
    expr = assignment->target();
  }

  AddFormalParameter(parameters, expr, initializer,
                     end_pos, is_rest);
}

void Parser::DeclareArrowFunctionFormalParameters(
    ParserFormalParameters* parameters, Expression* expr,
    const Scanner::Location& params_loc, Scanner::Location* duplicate_loc,
    bool* ok) {
  if (expr->IsEmptyParentheses()) return;

  AddArrowFunctionFormalParameters(parameters, expr, params_loc.end_pos,
                                   CHECK_OK_VOID);

  if (parameters->arity > Code::kMaxArguments) {
    ReportMessageAt(params_loc, MessageTemplate::kMalformedArrowFunParamList);
    *ok = false;
    return;
  }

  bool has_duplicate = false;
  DeclareFormalParameters(parameters->scope, parameters->params,
                          parameters->is_simple, &has_duplicate);
  if (has_duplicate) {
    *duplicate_loc = scanner()->location();
  }
  DCHECK_EQ(parameters->is_simple, parameters->scope->has_simple_parameters());
}

void Parser::PrepareGeneratorVariables() {
  // The code produced for generators relies on forced context allocation of
  // parameters (it does not restore the frame's parameters upon resume).
  function_state_->scope()->ForceContextAllocationForParameters();

  // Calling a generator returns a generator object.  That object is stored
  // in a temporary variable, a definition that is used by "yield"
  // expressions.
  function_state_->scope()->DeclareGeneratorObjectVar(
      ast_value_factory()->dot_generator_object_string());
}

FunctionLiteral* Parser::ParseFunctionLiteral(
    const AstRawString* function_name, Scanner::Location function_name_location,
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_pos, FunctionLiteral::FunctionType function_type,
    LanguageMode language_mode,
    ZoneList<const AstRawString*>* arguments_for_wrapped_function, bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'
  //
  // Getter ::
  //   '(' ')' '{' FunctionBody '}'
  //
  // Setter ::
  //   '(' PropertySetParameterList ')' '{' FunctionBody '}'

  bool is_wrapped = function_type == FunctionLiteral::kWrapped;
  DCHECK_EQ(is_wrapped, arguments_for_wrapped_function != nullptr);

  int pos = function_token_pos == kNoSourcePosition ? peek_position()
                                                    : function_token_pos;
  DCHECK_NE(kNoSourcePosition, pos);

  // Anonymous functions were passed either the empty symbol or a null
  // handle as the function name.  Remember if we were passed a non-empty
  // handle to decide whether to invoke function name inference.
  bool should_infer_name = function_name == nullptr;

  // We want a non-null handle as the function name by default. We will handle
  // the "function does not have a shared name" case later.
  if (should_infer_name) {
    function_name = ast_value_factory()->empty_string();
  }

  FunctionLiteral::EagerCompileHint eager_compile_hint =
      function_state_->next_function_is_likely_called() || is_wrapped
          ? FunctionLiteral::kShouldEagerCompile
          : default_eager_compile_hint();

  // Determine if the function can be parsed lazily. Lazy parsing is
  // different from lazy compilation; we need to parse more eagerly than we
  // compile.

  // We can only parse lazily if we also compile lazily. The heuristics for lazy
  // compilation are:
  // - It must not have been prohibited by the caller to Parse (some callers
  //   need a full AST).
  // - The outer scope must allow lazy compilation of inner functions.
  // - The function mustn't be a function expression with an open parenthesis
  //   before; we consider that a hint that the function will be called
  //   immediately, and it would be a waste of time to make it lazily
  //   compiled.
  // These are all things we can know at this point, without looking at the
  // function itself.

  // We separate between lazy parsing top level functions and lazy parsing inner
  // functions, because the latter needs to do more work. In particular, we need
  // to track unresolved variables to distinguish between these cases:
  // (function foo() {
  //   bar = function() { return 1; }
  //  })();
  // and
  // (function foo() {
  //   var a = 1;
  //   bar = function() { return a; }
  //  })();

  // Now foo will be parsed eagerly and compiled eagerly (optimization: assume
  // parenthesis before the function means that it will be called
  // immediately). bar can be parsed lazily, but we need to parse it in a mode
  // that tracks unresolved variables.
  DCHECK_IMPLIES(parse_lazily(), FLAG_lazy);
  DCHECK_IMPLIES(parse_lazily(), allow_lazy_);
  DCHECK_IMPLIES(parse_lazily(), extension_ == nullptr);

  const bool is_lazy =
      eager_compile_hint == FunctionLiteral::kShouldLazyCompile;
  const bool is_top_level = AllowsLazyParsingWithoutUnresolvedVariables();
  const bool is_lazy_top_level_function = is_lazy && is_top_level;
  const bool is_lazy_inner_function = is_lazy && !is_top_level;
  const bool is_expression =
      function_type == FunctionLiteral::kAnonymousExpression ||
      function_type == FunctionLiteral::kNamedExpression;

  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_,
      parsing_on_main_thread_
          ? RuntimeCallCounterId::kParseFunctionLiteral
          : RuntimeCallCounterId::kParseBackgroundFunctionLiteral);
  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();

  // Determine whether we can still lazy parse the inner function.
  // The preconditions are:
  // - Lazy compilation has to be enabled.
  // - Neither V8 natives nor native function declarations can be allowed,
  //   since parsing one would retroactively force the function to be
  //   eagerly compiled.
  // - The invoker of this parser can't depend on the AST being eagerly
  //   built (either because the function is about to be compiled, or
  //   because the AST is going to be inspected for some reason).
  // - Because of the above, we can't be attempting to parse a
  //   FunctionExpression; even without enclosing parentheses it might be
  //   immediately invoked.
  // - The function literal shouldn't be hinted to eagerly compile.

  // Inner functions will be parsed using a temporary Zone. After parsing, we
  // will migrate unresolved variable into a Scope in the main Zone.

  const bool should_preparse_inner =
      parse_lazily() && FLAG_lazy_inner_functions && is_lazy_inner_function &&
      (!is_expression || FLAG_aggressive_lazy_inner_functions);

  // This may be modified later to reflect preparsing decision taken
  bool should_preparse =
      (parse_lazily() && is_lazy_top_level_function) || should_preparse_inner;

  ZoneList<Statement*>* body = nullptr;
  int expected_property_count = -1;
  int suspend_count = -1;
  int num_parameters = -1;
  int function_length = -1;
  bool has_duplicate_parameters = false;
  int function_literal_id = GetNextFunctionLiteralId();
  ProducedPreParsedScopeData* produced_preparsed_scope_data = nullptr;

  Zone* outer_zone = zone();
  DeclarationScope* scope;

  {
    // Temporary zones can nest. When we migrate free variables (see below), we
    // need to recreate them in the previous Zone.
    AstNodeFactory previous_zone_ast_node_factory(ast_value_factory(), zone());

    // Open a new zone scope, which sets our AstNodeFactory to allocate in the
    // new temporary zone if the preconditions are satisfied, and ensures that
    // the previous zone is always restored after parsing the body. To be able
    // to do scope analysis correctly after full parsing, we migrate needed
    // information when the function is parsed.
    Zone temp_zone(zone()->allocator(), ZONE_NAME);
    DiscardableZoneScope zone_scope(this, &temp_zone, should_preparse);

    // This Scope lives in the main zone. We'll migrate data into that zone
    // later.
    scope = NewFunctionScope(kind, outer_zone);
    SetLanguageMode(scope, language_mode);
#ifdef DEBUG
    scope->SetScopeName(function_name);
    if (should_preparse) scope->set_needs_migration();
#endif

    if (!is_wrapped) Expect(Token::LPAREN, CHECK_OK);
    scope->set_start_position(scanner()->location().beg_pos);

    // Eager or lazy parse? If is_lazy_top_level_function, we'll parse
    // lazily. We'll call SkipFunction, which may decide to
    // abort lazy parsing if it suspects that wasn't a good idea. If so (in
    // which case the parser is expected to have backtracked), or if we didn't
    // try to lazy parse in the first place, we'll have to parse eagerly.
    if (should_preparse) {
      DCHECK(parse_lazily());
      DCHECK(is_lazy_top_level_function || is_lazy_inner_function);
      DCHECK(!is_wrapped);
      Scanner::BookmarkScope bookmark(scanner());
      bookmark.Set();
      LazyParsingResult result = SkipFunction(
          function_name, kind, function_type, scope, &num_parameters,
          &produced_preparsed_scope_data, is_lazy_inner_function,
          is_lazy_top_level_function, CHECK_OK);

      if (result == kLazyParsingAborted) {
        DCHECK(is_lazy_top_level_function);
        bookmark.Apply();
        // This is probably an initialization function. Inform the compiler it
        // should also eager-compile this function.
        eager_compile_hint = FunctionLiteral::kShouldEagerCompile;
        scope->ResetAfterPreparsing(ast_value_factory(), true);
        zone_scope.Reset();
        // Trigger eager (re-)parsing, just below this block.
        should_preparse = false;
      }
    }

    if (should_preparse) {
      scope->AnalyzePartially(&previous_zone_ast_node_factory);
    } else {
      body = ParseFunction(
          function_name, pos, kind, function_type, scope, &num_parameters,
          &function_length, &has_duplicate_parameters, &expected_property_count,
          &suspend_count, arguments_for_wrapped_function, CHECK_OK);
    }

    DCHECK_EQ(should_preparse, temp_zoned_);
    if (V8_UNLIKELY(FLAG_log_function_events)) {
      double ms = timer.Elapsed().InMillisecondsF();
      const char* event_name = should_preparse
                                   ? (is_top_level ? "preparse-no-resolution"
                                                   : "preparse-resolution")
                                   : "full-parse";
      logger_->FunctionEvent(
          event_name, nullptr, script_id(), ms, scope->start_position(),
          scope->end_position(),
          reinterpret_cast<const char*>(function_name->raw_data()),
          function_name->byte_length());
    }
    if (V8_UNLIKELY(FLAG_runtime_stats)) {
      if (should_preparse) {
        RuntimeCallCounterId counter_id =
            parsing_on_main_thread_
                ? RuntimeCallCounterId::kPreParseWithVariableResolution
                : RuntimeCallCounterId::
                      kPreParseBackgroundWithVariableResolution;
        if (is_top_level) {
          counter_id = parsing_on_main_thread_
                           ? RuntimeCallCounterId::kPreParseNoVariableResolution
                           : RuntimeCallCounterId::
                                 kPreParseBackgroundNoVariableResolution;
        }
        if (runtime_call_stats_) {
          runtime_call_stats_->CorrectCurrentCounterId(counter_id);
        }
      }
    }

    // Validate function name. We can do this only after parsing the function,
    // since the function can declare itself strict.
    language_mode = scope->language_mode();
    CheckFunctionName(language_mode, function_name, function_name_validity,
                      function_name_location, CHECK_OK);

    if (is_strict(language_mode)) {
      CheckStrictOctalLiteral(scope->start_position(), scope->end_position(),
                              CHECK_OK);
    }
    CheckConflictingVarDeclarations(scope, CHECK_OK);
  }  // DiscardableZoneScope goes out of scope.

  FunctionLiteral::ParameterFlag duplicate_parameters =
      has_duplicate_parameters ? FunctionLiteral::kHasDuplicateParameters
                               : FunctionLiteral::kNoDuplicateParameters;

  // Note that the FunctionLiteral needs to be created in the main Zone again.
  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      function_name, scope, body, expected_property_count, num_parameters,
      function_length, duplicate_parameters, function_type, eager_compile_hint,
      pos, true, function_literal_id, produced_preparsed_scope_data);
  function_literal->set_function_token_position(function_token_pos);
  function_literal->set_suspend_count(suspend_count);

  if (should_infer_name) {
    DCHECK_NOT_NULL(fni_);
    fni_->AddFunction(function_literal);
  }
  return function_literal;
}

Parser::LazyParsingResult Parser::SkipFunction(
    const AstRawString* function_name, FunctionKind kind,
    FunctionLiteral::FunctionType function_type,
    DeclarationScope* function_scope, int* num_parameters,
    ProducedPreParsedScopeData** produced_preparsed_scope_data,
    bool is_inner_function, bool may_abort, bool* ok) {
  FunctionState function_state(&function_state_, &scope_, function_scope);

  DCHECK_NE(kNoSourcePosition, function_scope->start_position());
  DCHECK_EQ(kNoSourcePosition, parameters_end_pos_);

  DCHECK_IMPLIES(IsArrowFunction(kind),
                 scanner()->current_token() == Token::ARROW);

  // FIXME(marja): There are 2 ways to skip functions now. Unify them.
  DCHECK_NOT_NULL(consumed_preparsed_scope_data_);
  if (consumed_preparsed_scope_data_->HasData()) {
    DCHECK(FLAG_preparser_scope_analysis);
    int end_position;
    LanguageMode language_mode;
    int num_inner_functions;
    bool uses_super_property;
    *produced_preparsed_scope_data =
        consumed_preparsed_scope_data_->GetDataForSkippableFunction(
            main_zone(), function_scope->start_position(), &end_position,
            num_parameters, &num_inner_functions, &uses_super_property,
            &language_mode);

    function_scope->outer_scope()->SetMustUsePreParsedScopeData();
    function_scope->set_is_skipped_function(true);
    function_scope->set_end_position(end_position);
    scanner()->SeekForward(end_position - 1);
    Expect(Token::RBRACE, CHECK_OK_VALUE(kLazyParsingComplete));
    SetLanguageMode(function_scope, language_mode);
    if (uses_super_property) {
      function_scope->RecordSuperPropertyUsage();
    }
    SkipFunctionLiterals(num_inner_functions);
    return kLazyParsingComplete;
  }

  // With no cached data, we partially parse the function, without building an
  // AST. This gathers the data needed to build a lazy function.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.PreParse");

  // Aborting inner function preparsing would leave scopes in an inconsistent
  // state; we don't parse inner functions in the abortable mode anyway.
  DCHECK(!is_inner_function || !may_abort);

  PreParser::PreParseResult result = reusable_preparser()->PreParseFunction(
      function_name, kind, function_type, function_scope, is_inner_function,
      may_abort, use_counts_, produced_preparsed_scope_data, this->script_id());

  // Return immediately if pre-parser decided to abort parsing.
  if (result == PreParser::kPreParseAbort) return kLazyParsingAborted;
  if (result == PreParser::kPreParseStackOverflow) {
    // Propagate stack overflow.
    set_stack_overflow();
    *ok = false;
    return kLazyParsingComplete;
  }
  if (pending_error_handler()->has_pending_error()) {
    *ok = false;
    return kLazyParsingComplete;
  }

  set_allow_eval_cache(reusable_preparser()->allow_eval_cache());

  PreParserLogger* logger = reusable_preparser()->logger();
  function_scope->set_end_position(logger->end());
  Expect(Token::RBRACE, CHECK_OK_VALUE(kLazyParsingComplete));
  total_preparse_skipped_ +=
      function_scope->end_position() - function_scope->start_position();
  *num_parameters = logger->num_parameters();
  SkipFunctionLiterals(logger->num_inner_functions());
  return kLazyParsingComplete;
}

Statement* Parser::BuildAssertIsCoercible(Variable* var,
                                          ObjectLiteral* pattern) {
  // if (var === null || var === undefined)
  //     throw /* type error kNonCoercible) */;
  auto source_position = pattern->position();
  const AstRawString* property = ast_value_factory()->empty_string();
  MessageTemplate::Template msg = MessageTemplate::kNonCoercible;
  for (ObjectLiteralProperty* literal_property : *pattern->properties()) {
    Expression* key = literal_property->key();
    if (key->IsPropertyName()) {
      property = key->AsLiteral()->AsRawPropertyName();
      msg = MessageTemplate::kNonCoercibleWithProperty;
      source_position = key->position();
      break;
    }
  }

  Expression* condition = factory()->NewBinaryOperation(
      Token::OR,
      factory()->NewCompareOperation(
          Token::EQ_STRICT, factory()->NewVariableProxy(var),
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition),
      factory()->NewCompareOperation(
          Token::EQ_STRICT, factory()->NewVariableProxy(var),
          factory()->NewNullLiteral(kNoSourcePosition), kNoSourcePosition),
      kNoSourcePosition);
  Expression* throw_type_error =
      NewThrowTypeError(msg, property, source_position);
  IfStatement* if_statement = factory()->NewIfStatement(
      condition,
      factory()->NewExpressionStatement(throw_type_error, kNoSourcePosition),
      factory()->NewEmptyStatement(kNoSourcePosition), kNoSourcePosition);
  return if_statement;
}

class InitializerRewriter final
    : public AstTraversalVisitor<InitializerRewriter> {
 public:
  InitializerRewriter(uintptr_t stack_limit, Expression* root, Parser* parser)
      : AstTraversalVisitor(stack_limit, root), parser_(parser) {}

 private:
  // This is required so that the overriden Visit* methods can be
  // called by the base class (template).
  friend class AstTraversalVisitor<InitializerRewriter>;

  // Just rewrite destructuring assignments wrapped in RewritableExpressions.
  void VisitRewritableExpression(RewritableExpression* to_rewrite) {
    if (to_rewrite->is_rewritten()) return;
    parser_->RewriteDestructuringAssignment(to_rewrite);
    AstTraversalVisitor::VisitRewritableExpression(to_rewrite);
  }

  // Code in function literals does not need to be eagerly rewritten, it will be
  // rewritten when scheduled.
  void VisitFunctionLiteral(FunctionLiteral* expr) {}

  Parser* parser_;
};

void Parser::RewriteParameterInitializer(Expression* expr) {
  InitializerRewriter rewriter(stack_limit_, expr, this);
  rewriter.Run();
}


Block* Parser::BuildParameterInitializationBlock(
    const ParserFormalParameters& parameters, bool* ok) {
  DCHECK(!parameters.is_simple);
  DCHECK(scope()->is_function_scope());
  DCHECK_EQ(scope(), parameters.scope);
  Block* init_block = factory()->NewBlock(1, true);
  int index = 0;
  for (auto parameter : parameters.params) {
    DeclarationDescriptor descriptor;
    descriptor.declaration_kind = DeclarationDescriptor::PARAMETER;
    descriptor.scope = scope();
    descriptor.mode = LET;
    descriptor.declaration_pos = parameter->pattern->position();
    // The position that will be used by the AssignmentExpression
    // which copies from the temp parameter to the pattern.
    //
    // TODO(adamk): Should this be kNoSourcePosition, since
    // it's just copying from a temp var to the real param var?
    descriptor.initialization_pos = parameter->pattern->position();
    Expression* initial_value =
        factory()->NewVariableProxy(parameters.scope->parameter(index));
    if (parameter->initializer != nullptr) {
      // IS_UNDEFINED($param) ? initializer : $param

      // Ensure initializer is rewritten
      RewriteParameterInitializer(parameter->initializer);

      auto condition = factory()->NewCompareOperation(
          Token::EQ_STRICT,
          factory()->NewVariableProxy(parameters.scope->parameter(index)),
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);
      initial_value = factory()->NewConditional(
          condition, parameter->initializer, initial_value, kNoSourcePosition);
      descriptor.initialization_pos = parameter->initializer->position();
    }

    Scope* param_scope = scope();
    Block* param_block = init_block;
    if (!parameter->is_simple() &&
        scope()->AsDeclarationScope()->calls_sloppy_eval()) {
      param_scope = NewVarblockScope();
      param_scope->set_start_position(descriptor.initialization_pos);
      param_scope->set_end_position(parameter->initializer_end_position);
      param_scope->RecordEvalCall();
      param_block = factory()->NewBlock(8, true);
      param_block->set_scope(param_scope);
      // Pass the appropriate scope in so that PatternRewriter can appropriately
      // rewrite inner initializers of the pattern to param_scope
      descriptor.scope = param_scope;
      // Rewrite the outer initializer to point to param_scope
      ReparentExpressionScope(stack_limit(), initial_value, param_scope);
    }

    BlockState block_state(&scope_, param_scope);
    DeclarationParsingResult::Declaration decl(
        parameter->pattern, parameter->initializer_end_position, initial_value);
    DeclareAndInitializeVariables(param_block, &descriptor, &decl, nullptr,
                                  CHECK_OK);

    if (param_block != init_block) {
      param_scope = param_scope->FinalizeBlockScope();
      if (param_scope != nullptr) {
        CheckConflictingVarDeclarations(param_scope, CHECK_OK);
      }
      init_block->statements()->Add(param_block, zone());
    }
    ++index;
  }
  return init_block;
}

Scope* Parser::NewHiddenCatchScope() {
  Scope* catch_scope = NewScopeWithParent(scope(), CATCH_SCOPE);
  catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(), VAR);
  catch_scope->set_is_hidden();
  return catch_scope;
}

Block* Parser::BuildRejectPromiseOnException(Block* inner_block) {
  // .promise = %AsyncFunctionPromiseCreate();
  // try {
  //   <inner_block>
  // } catch (.catch) {
  //   %RejectPromise(.promise, .catch);
  //   return .promise;
  // } finally {
  //   %AsyncFunctionPromiseRelease(.promise);
  // }
  Block* result = factory()->NewBlock(2, true);

  // .promise = %AsyncFunctionPromiseCreate();
  Statement* set_promise;
  {
    Expression* create_promise = factory()->NewCallRuntime(
        Context::ASYNC_FUNCTION_PROMISE_CREATE_INDEX,
        new (zone()) ZoneList<Expression*>(0, zone()), kNoSourcePosition);
    Assignment* assign_promise = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(PromiseVariable()),
        create_promise, kNoSourcePosition);
    set_promise =
        factory()->NewExpressionStatement(assign_promise, kNoSourcePosition);
  }
  result->statements()->Add(set_promise, zone());

  // catch (.catch) { return %RejectPromise(.promise, .catch), .promise }
  Scope* catch_scope = NewHiddenCatchScope();

  Expression* promise_reject = BuildRejectPromise(
      factory()->NewVariableProxy(catch_scope->catch_variable()),
      kNoSourcePosition);
  Block* catch_block = IgnoreCompletion(
      factory()->NewReturnStatement(promise_reject, kNoSourcePosition));

  TryStatement* try_catch_statement =
      factory()->NewTryCatchStatementForAsyncAwait(
          inner_block, catch_scope, catch_block, kNoSourcePosition);

  // There is no TryCatchFinally node, so wrap it in an outer try/finally
  Block* outer_try_block = IgnoreCompletion(try_catch_statement);

  // finally { %AsyncFunctionPromiseRelease(.promise) }
  Block* finally_block;
  {
    ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(1, zone());
    args->Add(factory()->NewVariableProxy(PromiseVariable()), zone());
    Expression* call_promise_release = factory()->NewCallRuntime(
        Context::ASYNC_FUNCTION_PROMISE_RELEASE_INDEX, args, kNoSourcePosition);
    Statement* promise_release = factory()->NewExpressionStatement(
        call_promise_release, kNoSourcePosition);
    finally_block = IgnoreCompletion(promise_release);
  }

  Statement* try_finally_statement = factory()->NewTryFinallyStatement(
      outer_try_block, finally_block, kNoSourcePosition);

  result->statements()->Add(try_finally_statement, zone());
  return result;
}

Expression* Parser::BuildResolvePromise(Expression* value, int pos) {
  // %ResolvePromise(.promise, value), .promise
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(factory()->NewVariableProxy(PromiseVariable()), zone());
  args->Add(value, zone());
  Expression* call_runtime =
      factory()->NewCallRuntime(Runtime::kInlineResolvePromise, args, pos);
  return factory()->NewBinaryOperation(
      Token::COMMA, call_runtime,
      factory()->NewVariableProxy(PromiseVariable()), pos);
}

Expression* Parser::BuildRejectPromise(Expression* value, int pos) {
  // %promise_internal_reject(.promise, value, false), .promise
  // Disables the additional debug event for the rejection since a debug event
  // already happened for the exception that got us here.
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(3, zone());
  args->Add(factory()->NewVariableProxy(PromiseVariable()), zone());
  args->Add(value, zone());
  args->Add(factory()->NewBooleanLiteral(false, pos), zone());
  Expression* call_runtime =
      factory()->NewCallRuntime(Runtime::kInlineRejectPromise, args, pos);
  return factory()->NewBinaryOperation(
      Token::COMMA, call_runtime,
      factory()->NewVariableProxy(PromiseVariable()), pos);
}

Variable* Parser::PromiseVariable() {
  // Based on the various compilation paths, there are many different code
  // paths which may be the first to access the Promise temporary. Whichever
  // comes first should create it and stash it in the FunctionState.
  Variable* promise = function_state_->scope()->promise_var();
  if (promise == nullptr) {
    promise = function_state_->scope()->DeclarePromiseVar(
        ast_value_factory()->empty_string());
  }
  return promise;
}

Expression* Parser::BuildInitialYield(int pos, FunctionKind kind) {
  Expression* yield_result = factory()->NewVariableProxy(
      function_state_->scope()->generator_object_var());
  // The position of the yield is important for reporting the exception
  // caused by calling the .throw method on a generator suspended at the
  // initial yield (i.e. right after generator instantiation).
  function_state_->AddSuspend();
  return factory()->NewYield(yield_result, scope()->start_position(),
                             Suspend::kOnExceptionThrow);
}

ZoneList<Statement*>* Parser::ParseFunction(
    const AstRawString* function_name, int pos, FunctionKind kind,
    FunctionLiteral::FunctionType function_type,
    DeclarationScope* function_scope, int* num_parameters, int* function_length,
    bool* has_duplicate_parameters, int* expected_property_count,
    int* suspend_count,
    ZoneList<const AstRawString*>* arguments_for_wrapped_function, bool* ok) {
  ParsingModeScope mode(this, allow_lazy_ ? PARSE_LAZILY : PARSE_EAGERLY);

  FunctionState function_state(&function_state_, &scope_, function_scope);

  bool is_wrapped = function_type == FunctionLiteral::kWrapped;

  DuplicateFinder duplicate_finder;
  ExpressionClassifier formals_classifier(this, &duplicate_finder);

  int expected_parameters_end_pos = parameters_end_pos_;
  if (expected_parameters_end_pos != kNoSourcePosition) {
    // This is the first function encountered in a CreateDynamicFunction eval.
    parameters_end_pos_ = kNoSourcePosition;
    // The function name should have been ignored, giving us the empty string
    // here.
    DCHECK_EQ(function_name, ast_value_factory()->empty_string());
  }

  ParserFormalParameters formals(function_scope);

  if (is_wrapped) {
    // For a function implicitly wrapped in function header and footer, the
    // function arguments are provided separately to the source, and are
    // declared directly here.
    int arguments_length = arguments_for_wrapped_function->length();
    for (int i = 0; i < arguments_length; i++) {
      const bool is_rest = false;
      Expression* argument = ExpressionFromIdentifier(
          arguments_for_wrapped_function->at(i), kNoSourcePosition);
      AddFormalParameter(&formals, argument, NullExpression(),
                         kNoSourcePosition, is_rest);
    }
    DCHECK_EQ(arguments_length, formals.num_parameters());
    DeclareFormalParameters(formals.scope, formals.params, formals.is_simple);
  } else {
    // For a regular function, the function arguments are parsed from source.
    DCHECK_NULL(arguments_for_wrapped_function);
    ParseFormalParameterList(&formals, CHECK_OK);
    if (expected_parameters_end_pos != kNoSourcePosition) {
      // Check for '(' or ')' shenanigans in the parameter string for dynamic
      // functions.
      int position = peek_position();
      if (position < expected_parameters_end_pos) {
        ReportMessageAt(Scanner::Location(position, position + 1),
                        MessageTemplate::kArgStringTerminatesParametersEarly);
        *ok = false;
        return nullptr;
      } else if (position > expected_parameters_end_pos) {
        ReportMessageAt(Scanner::Location(expected_parameters_end_pos - 2,
                                          expected_parameters_end_pos),
                        MessageTemplate::kUnexpectedEndOfArgString);
        *ok = false;
        return nullptr;
      }
    }
    Expect(Token::RPAREN, CHECK_OK);
    int formals_end_position = scanner()->location().end_pos;

    CheckArityRestrictions(formals.arity, kind, formals.has_rest,
                           function_scope->start_position(),
                           formals_end_position, CHECK_OK);
    Expect(Token::LBRACE, CHECK_OK);
  }
  *num_parameters = formals.num_parameters();
  *function_length = formals.function_length;

  ZoneList<Statement*>* body = new (zone()) ZoneList<Statement*>(8, zone());
  ParseFunctionBody(body, function_name, pos, formals, kind, function_type, ok);

  // Validate parameter names. We can do this only after parsing the function,
  // since the function can declare itself strict.
  const bool allow_duplicate_parameters =
      is_sloppy(function_scope->language_mode()) && formals.is_simple &&
      !IsConciseMethod(kind);
  ValidateFormalParameters(function_scope->language_mode(),
                           allow_duplicate_parameters, CHECK_OK);

  RewriteDestructuringAssignments();

  *has_duplicate_parameters =
      !classifier()->is_valid_formal_parameter_list_without_duplicates();

  *expected_property_count = function_state.expected_property_count();
  *suspend_count = function_state.suspend_count();
  return body;
}

void Parser::DeclareClassVariable(const AstRawString* name,
                                  ClassInfo* class_info, int class_token_pos,
                                  bool* ok) {
#ifdef DEBUG
  scope()->SetScopeName(name);
#endif

  if (name != nullptr) {
    VariableProxy* proxy = factory()->NewVariableProxy(name, NORMAL_VARIABLE);
    Declaration* declaration =
        factory()->NewVariableDeclaration(proxy, class_token_pos);
    class_info->variable =
        Declare(declaration, DeclarationDescriptor::NORMAL, CONST,
                Variable::DefaultInitializationFlag(CONST), ok);
  }
}

// TODO(gsathya): Ideally, this should just bypass scope analysis and
// allocate a slot directly on the context. We should just store this
// index in the AST, instead of storing the variable.
Variable* Parser::CreateSyntheticContextVariable(const AstRawString* name,
                                                 bool* ok) {
  VariableProxy* proxy = factory()->NewVariableProxy(name, NORMAL_VARIABLE);
  Declaration* declaration =
      factory()->NewVariableDeclaration(proxy, kNoSourcePosition);
  Variable* var = Declare(declaration, DeclarationDescriptor::NORMAL, CONST,
                          Variable::DefaultInitializationFlag(CONST), CHECK_OK);
  var->ForceContextAllocation();
  return var;
}

// This method declares a property of the given class.  It updates the
// following fields of class_info, as appropriate:
//   - constructor
//   - properties
void Parser::DeclareClassProperty(const AstRawString* class_name,
                                  ClassLiteralProperty* property,
                                  const AstRawString* property_name,
                                  ClassLiteralProperty::Kind kind,
                                  bool is_static, bool is_constructor,
                                  bool is_computed_name, ClassInfo* class_info,
                                  bool* ok) {
  if (is_constructor) {
    DCHECK(!class_info->constructor);
    class_info->constructor = property->value()->AsFunctionLiteral();
    DCHECK_NOT_NULL(class_info->constructor);
    class_info->constructor->set_raw_name(
        class_name != nullptr ? ast_value_factory()->NewConsString(class_name)
                              : nullptr);
    return;
  }

  if (kind != ClassLiteralProperty::PUBLIC_FIELD &&
      kind != ClassLiteralProperty::PRIVATE_FIELD) {
    class_info->properties->Add(property, zone());
    return;
  }

  DCHECK(allow_harmony_public_fields() || allow_harmony_private_fields());

  if (is_static) {
    DCHECK(allow_harmony_static_fields());
    DCHECK_EQ(kind, ClassLiteralProperty::PUBLIC_FIELD);
    class_info->static_fields->Add(property, zone());
  } else {
    class_info->instance_fields->Add(property, zone());
  }

  if (is_computed_name) {
    DCHECK_EQ(kind, ClassLiteralProperty::PUBLIC_FIELD);
    // We create a synthetic variable name here so that scope
    // analysis doesn't dedupe the vars.
    Variable* computed_name_var = CreateSyntheticContextVariable(
        ClassFieldVariableName(ast_value_factory(),
                               class_info->computed_field_count),
        CHECK_OK_VOID);
    property->set_computed_name_var(computed_name_var);
    class_info->properties->Add(property, zone());
  }

  if (kind == ClassLiteralProperty::PRIVATE_FIELD) {
    Variable* private_field_name_var =
        CreateSyntheticContextVariable(property_name, CHECK_OK_VOID);
    property->set_private_field_name_var(private_field_name_var);
    class_info->properties->Add(property, zone());
  }
}

FunctionLiteral* Parser::CreateInitializerFunction(
    DeclarationScope* scope, ZoneList<ClassLiteral::Property*>* fields) {
  DCHECK_EQ(scope->function_kind(),
            FunctionKind::kClassFieldsInitializerFunction);
  // function() { .. class fields initializer .. }
  ZoneList<Statement*>* statements = NewStatementList(1);
  InitializeClassFieldsStatement* static_fields =
      factory()->NewInitializeClassFieldsStatement(fields, kNoSourcePosition);
  statements->Add(static_fields, zone());
  return factory()->NewFunctionLiteral(
      ast_value_factory()->empty_string(), scope, statements, 0, 0, 0,
      FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::kAnonymousExpression,
      FunctionLiteral::kShouldEagerCompile, scope->start_position(), true,
      GetNextFunctionLiteralId());
}

// This method generates a ClassLiteral AST node.
// It uses the following fields of class_info:
//   - constructor (if missing, it updates it with a default constructor)
//   - proxy
//   - extends
//   - properties
//   - has_name_static_property
//   - has_static_computed_names
Expression* Parser::RewriteClassLiteral(Scope* block_scope,
                                        const AstRawString* name,
                                        ClassInfo* class_info, int pos,
                                        int end_pos, bool* ok) {
  DCHECK_NOT_NULL(block_scope);
  DCHECK_EQ(block_scope->scope_type(), BLOCK_SCOPE);
  DCHECK_EQ(block_scope->language_mode(), LanguageMode::kStrict);

  bool has_extends = class_info->extends != nullptr;
  bool has_default_constructor = class_info->constructor == nullptr;
  if (has_default_constructor) {
    class_info->constructor =
        DefaultConstructor(name, has_extends, pos, end_pos);
  }

  if (name != nullptr) {
    DCHECK_NOT_NULL(class_info->variable);
    class_info->variable->set_initializer_position(end_pos);
  }

  FunctionLiteral* static_fields_initializer = nullptr;
  if (class_info->has_static_class_fields) {
    static_fields_initializer = CreateInitializerFunction(
        class_info->static_fields_scope, class_info->static_fields);
  }

  FunctionLiteral* instance_fields_initializer_function = nullptr;
  if (class_info->has_instance_class_fields) {
    instance_fields_initializer_function = CreateInitializerFunction(
        class_info->instance_fields_scope, class_info->instance_fields);
    class_info->constructor->set_requires_instance_fields_initializer(true);
  }

  ClassLiteral* class_literal = factory()->NewClassLiteral(
      block_scope, class_info->variable, class_info->extends,
      class_info->constructor, class_info->properties,
      static_fields_initializer, instance_fields_initializer_function, pos,
      end_pos, class_info->has_name_static_property,
      class_info->has_static_computed_names, class_info->is_anonymous);

  AddFunctionForNameInference(class_info->constructor);
  return class_literal;
}

void Parser::CheckConflictingVarDeclarations(Scope* scope, bool* ok) {
  Declaration* decl = scope->CheckConflictingVarDeclarations();
  if (decl != nullptr) {
    // In ES6, conflicting variable bindings are early errors.
    const AstRawString* name = decl->proxy()->raw_name();
    int position = decl->proxy()->position();
    Scanner::Location location =
        position == kNoSourcePosition
            ? Scanner::Location::invalid()
            : Scanner::Location(position, position + 1);
    ReportMessageAt(location, MessageTemplate::kVarRedeclaration, name);
    *ok = false;
  }
}

bool Parser::IsPropertyWithPrivateFieldKey(Expression* expression) {
  if (!expression->IsProperty()) return false;
  Property* property = expression->AsProperty();

  if (!property->key()->IsVariableProxy()) return false;
  VariableProxy* key = property->key()->AsVariableProxy();

  return key->is_private_field();
}

void Parser::InsertShadowingVarBindingInitializers(Block* inner_block) {
  // For each var-binding that shadows a parameter, insert an assignment
  // initializing the variable with the parameter.
  Scope* inner_scope = inner_block->scope();
  DCHECK(inner_scope->is_declaration_scope());
  Scope* function_scope = inner_scope->outer_scope();
  DCHECK(function_scope->is_function_scope());
  BlockState block_state(&scope_, inner_scope);
  for (Declaration* decl : *inner_scope->declarations()) {
    if (decl->proxy()->var()->mode() != VAR || !decl->IsVariableDeclaration()) {
      continue;
    }
    const AstRawString* name = decl->proxy()->raw_name();
    Variable* parameter = function_scope->LookupLocal(name);
    if (parameter == nullptr) continue;
    VariableProxy* to = NewUnresolved(name);
    VariableProxy* from = factory()->NewVariableProxy(parameter);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, to, from, kNoSourcePosition);
    Statement* statement =
        factory()->NewExpressionStatement(assignment, kNoSourcePosition);
    inner_block->statements()->InsertAt(0, statement, zone());
  }
}

void Parser::InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope) {
  // For the outermost eval scope, we cannot hoist during parsing: let
  // declarations in the surrounding scope may prevent hoisting, but the
  // information is unaccessible during parsing. In this case, we hoist later in
  // DeclarationScope::Analyze.
  if (scope->is_eval_scope() && scope->outer_scope() == original_scope_) {
    return;
  }
  scope->HoistSloppyBlockFunctions(factory());
}

// ----------------------------------------------------------------------------
// Parser support

bool Parser::TargetStackContainsLabel(const AstRawString* label) {
  for (ParserTarget* t = target_stack_; t != nullptr; t = t->previous()) {
    if (ContainsLabel(t->statement()->labels(), label)) return true;
  }
  return false;
}


BreakableStatement* Parser::LookupBreakTarget(const AstRawString* label,
                                              bool* ok) {
  bool anonymous = label == nullptr;
  for (ParserTarget* t = target_stack_; t != nullptr; t = t->previous()) {
    BreakableStatement* stat = t->statement();
    if ((anonymous && stat->is_target_for_anonymous()) ||
        (!anonymous && ContainsLabel(stat->labels(), label))) {
      return stat;
    }
  }
  return nullptr;
}


IterationStatement* Parser::LookupContinueTarget(const AstRawString* label,
                                                 bool* ok) {
  bool anonymous = label == nullptr;
  for (ParserTarget* t = target_stack_; t != nullptr; t = t->previous()) {
    IterationStatement* stat = t->statement()->AsIterationStatement();
    if (stat == nullptr) continue;

    DCHECK(stat->is_target_for_anonymous());
    if (anonymous || ContainsLabel(stat->labels(), label)) {
      return stat;
    }
  }
  return nullptr;
}


void Parser::HandleSourceURLComments(Isolate* isolate, Handle<Script> script) {
  Handle<String> source_url = scanner_.SourceUrl(isolate);
  if (!source_url.is_null()) {
    script->set_source_url(*source_url);
  }
  Handle<String> source_mapping_url = scanner_.SourceMappingUrl(isolate);
  if (!source_mapping_url.is_null()) {
    script->set_source_mapping_url(*source_mapping_url);
  }
}

void Parser::UpdateStatistics(Isolate* isolate, Handle<Script> script) {
  // Move statistics to Isolate.
  for (int feature = 0; feature < v8::Isolate::kUseCounterFeatureCount;
       ++feature) {
    if (use_counts_[feature] > 0) {
      isolate->CountUsage(v8::Isolate::UseCounterFeature(feature));
    }
  }
  if (scanner_.FoundHtmlComment()) {
    isolate->CountUsage(v8::Isolate::kHtmlComment);
    if (script->line_offset() == 0 && script->column_offset() == 0) {
      isolate->CountUsage(v8::Isolate::kHtmlCommentInExternalScript);
    }
  }
  isolate->counters()->total_preparse_skipped()->Increment(
      total_preparse_skipped_);
}

void Parser::ParseOnBackground(ParseInfo* info) {
  RuntimeCallTimerScope runtimeTimer(
      runtime_call_stats_, RuntimeCallCounterId::kParseBackgroundProgram);
  parsing_on_main_thread_ = false;
  if (!info->script().is_null()) {
    set_script_id(info->script()->id());
  }

  DCHECK_NULL(info->literal());
  FunctionLiteral* result = nullptr;

  scanner_.Initialize(info->character_stream(), info->is_module());
  DCHECK(info->maybe_outer_scope_info().is_null());

  DCHECK(original_scope_);

  // When streaming, we don't know the length of the source until we have parsed
  // it. The raw data can be UTF-8, so we wouldn't know the source length until
  // we have decoded it anyway even if we knew the raw data length (which we
  // don't). We work around this by storing all the scopes which need their end
  // position set at the end of the script (the top scope and possible eval
  // scopes) and set their end position after we know the script length.
  if (info->is_toplevel()) {
    fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());
    result = DoParseProgram(info);
  } else {
    result = DoParseFunction(info, info->function_name());
  }
  MaybeResetCharacterStream(info, result);

  info->set_literal(result);

  // We cannot internalize on a background thread; a foreground task will take
  // care of calling AstValueFactory::Internalize just before compilation.
}

Parser::TemplateLiteralState Parser::OpenTemplateLiteral(int pos) {
  return new (zone()) TemplateLiteral(zone(), pos);
}

void Parser::AddTemplateSpan(TemplateLiteralState* state, bool should_cook,
                             bool tail) {
  int end = scanner()->location().end_pos - (tail ? 1 : 2);
  const AstRawString* raw = scanner()->CurrentRawSymbol(ast_value_factory());
  if (should_cook) {
    const AstRawString* cooked = scanner()->CurrentSymbol(ast_value_factory());
    (*state)->AddTemplateSpan(cooked, raw, end, zone());
  } else {
    (*state)->AddTemplateSpan(nullptr, raw, end, zone());
  }
}


void Parser::AddTemplateExpression(TemplateLiteralState* state,
                                   Expression* expression) {
  (*state)->AddExpression(expression, zone());
}


Expression* Parser::CloseTemplateLiteral(TemplateLiteralState* state, int start,
                                         Expression* tag) {
  TemplateLiteral* lit = *state;
  int pos = lit->position();
  const ZoneList<const AstRawString*>* cooked_strings = lit->cooked();
  const ZoneList<const AstRawString*>* raw_strings = lit->raw();
  const ZoneList<Expression*>* expressions = lit->expressions();
  DCHECK_EQ(cooked_strings->length(), raw_strings->length());
  DCHECK_EQ(cooked_strings->length(), expressions->length() + 1);

  if (!tag) {
    if (cooked_strings->length() == 1) {
      return factory()->NewStringLiteral(cooked_strings->first(), pos);
    }
    return factory()->NewTemplateLiteral(cooked_strings, expressions, pos);
  } else {
    // GetTemplateObject
    Expression* template_object =
        factory()->NewGetTemplateObject(cooked_strings, raw_strings, pos);

    // Call TagFn
    ZoneList<Expression*>* call_args =
        new (zone()) ZoneList<Expression*>(expressions->length() + 1, zone());
    call_args->Add(template_object, zone());
    call_args->AddAll(*expressions, zone());
    return factory()->NewTaggedTemplate(tag, call_args, pos);
  }
}

namespace {

bool OnlyLastArgIsSpread(ZoneList<Expression*>* args) {
  for (int i = 0; i < args->length() - 1; i++) {
    if (args->at(i)->IsSpread()) {
      return false;
    }
  }
  return args->at(args->length() - 1)->IsSpread();
}

}  // namespace

ArrayLiteral* Parser::ArrayLiteralFromListWithSpread(
    ZoneList<Expression*>* list) {
  // If there's only a single spread argument, a fast path using CallWithSpread
  // is taken.
  DCHECK_LT(1, list->length());

  // The arguments of the spread call become a single ArrayLiteral.
  int first_spread = 0;
  for (; first_spread < list->length() && !list->at(first_spread)->IsSpread();
       ++first_spread) {
  }

  DCHECK_LT(first_spread, list->length());
  return factory()->NewArrayLiteral(list, first_spread, kNoSourcePosition);
}

Expression* Parser::SpreadCall(Expression* function,
                               ZoneList<Expression*>* args_list, int pos,
                               Call::PossiblyEval is_possibly_eval) {
  // Handle this case in BytecodeGenerator.
  if (OnlyLastArgIsSpread(args_list)) {
    return factory()->NewCall(function, args_list, pos);
  }

  if (function->IsSuperCallReference()) {
    // Super calls
    // $super_constructor = %_GetSuperConstructor(<this-function>)
    // %reflect_construct($super_constructor, args, new.target)
    ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(3, zone());
    ZoneList<Expression*>* tmp = new (zone()) ZoneList<Expression*>(1, zone());
    tmp->Add(function->AsSuperCallReference()->this_function_var(), zone());
    args->Add(factory()->NewCallRuntime(Runtime::kInlineGetSuperConstructor,
                                        tmp, pos),
              zone());
    args->Add(ArrayLiteralFromListWithSpread(args_list), zone());
    args->Add(function->AsSuperCallReference()->new_target_var(), zone());
    return factory()->NewCallRuntime(Context::REFLECT_CONSTRUCT_INDEX, args,
                                     pos);
  } else {
    ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(3, zone());
    if (function->IsProperty()) {
      // Method calls
      if (function->AsProperty()->IsSuperAccess()) {
        Expression* home = ThisExpression(kNoSourcePosition);
        args->Add(function, zone());
        args->Add(home, zone());
      } else {
        Variable* temp = NewTemporary(ast_value_factory()->empty_string());
        VariableProxy* obj = factory()->NewVariableProxy(temp);
        Assignment* assign_obj = factory()->NewAssignment(
            Token::ASSIGN, obj, function->AsProperty()->obj(),
            kNoSourcePosition);
        function = factory()->NewProperty(
            assign_obj, function->AsProperty()->key(), kNoSourcePosition);
        args->Add(function, zone());
        obj = factory()->NewVariableProxy(temp);
        args->Add(obj, zone());
      }
    } else {
      // Non-method calls
      args->Add(function, zone());
      args->Add(factory()->NewUndefinedLiteral(kNoSourcePosition), zone());
    }
    args->Add(ArrayLiteralFromListWithSpread(args_list), zone());
    return factory()->NewCallRuntime(Context::REFLECT_APPLY_INDEX, args, pos);
  }
}

Expression* Parser::SpreadCallNew(Expression* function,
                                  ZoneList<Expression*>* args_list, int pos) {
  if (OnlyLastArgIsSpread(args_list)) {
    // Handle in BytecodeGenerator.
    return factory()->NewCallNew(function, args_list, pos);
  }
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(function, zone());
  args->Add(ArrayLiteralFromListWithSpread(args_list), zone());

  return factory()->NewCallRuntime(Context::REFLECT_CONSTRUCT_INDEX, args, pos);
}


void Parser::SetLanguageMode(Scope* scope, LanguageMode mode) {
  v8::Isolate::UseCounterFeature feature;
  if (is_sloppy(mode))
    feature = v8::Isolate::kSloppyMode;
  else if (is_strict(mode))
    feature = v8::Isolate::kStrictMode;
  else
    UNREACHABLE();
  ++use_counts_[feature];
  scope->SetLanguageMode(mode);
}

void Parser::SetAsmModule() {
  // Store the usage count; The actual use counter on the isolate is
  // incremented after parsing is done.
  ++use_counts_[v8::Isolate::kUseAsm];
  DCHECK(scope()->is_declaration_scope());
  scope()->AsDeclarationScope()->set_asm_module();
}

Expression* Parser::ExpressionListToExpression(ZoneList<Expression*>* args) {
  Expression* expr = args->at(0);
  for (int i = 1; i < args->length(); ++i) {
    expr = factory()->NewBinaryOperation(Token::COMMA, expr, args->at(i),
                                         expr->position());
  }
  return expr;
}

// This method completes the desugaring of the body of async_function.
void Parser::RewriteAsyncFunctionBody(ZoneList<Statement*>* body, Block* block,
                                      Expression* return_value, bool* ok) {
  // function async_function() {
  //   .generator_object = %CreateJSGeneratorObject();
  //   BuildRejectPromiseOnException({
  //     ... block ...
  //     return %ResolvePromise(.promise, expr), .promise;
  //   })
  // }

  return_value = BuildResolvePromise(return_value, return_value->position());
  block->statements()->Add(
      factory()->NewReturnStatement(return_value, return_value->position()),
      zone());
  block = BuildRejectPromiseOnException(block);
  body->Add(block, zone());
}

void Parser::RewriteDestructuringAssignments() {
  const auto& assignments =
      function_state_->destructuring_assignments_to_rewrite();
  for (int i = assignments.length() - 1; i >= 0; --i) {
    // Rewrite list in reverse, so that nested assignment patterns are rewritten
    // correctly.
    RewritableExpression* to_rewrite = assignments[i];
    DCHECK_NOT_NULL(to_rewrite);
    if (!to_rewrite->is_rewritten()) {
      // Since this function is called at the end of parsing the program,
      // pair.scope may already have been removed by FinalizeBlockScope in the
      // meantime.
      Scope* scope = to_rewrite->scope()->GetUnremovedScope();
      // Scope at the time of the rewriting and the original parsing
      // should be in the same function.
      DCHECK(scope->GetClosureScope() == scope_->GetClosureScope());
      BlockState block_state(&scope_, scope);
      RewriteDestructuringAssignment(to_rewrite);
    }
  }
}

void Parser::QueueDestructuringAssignmentForRewriting(
    RewritableExpression* expr) {
  function_state_->AddDestructuringAssignment(expr);
}

void Parser::SetFunctionNameFromPropertyName(LiteralProperty* property,
                                             const AstRawString* name,
                                             const AstRawString* prefix) {
  // Ensure that the function we are going to create has shared name iff
  // we are not going to set it later.
  if (property->NeedsSetFunctionName()) {
    name = nullptr;
    prefix = nullptr;
  } else {
    // If the property value is an anonymous function or an anonymous class or
    // a concise method or an accessor function which doesn't require the name
    // to be set then the shared name must be provided.
    DCHECK_IMPLIES(property->value()->IsAnonymousFunctionDefinition() ||
                       property->value()->IsConciseMethodDefinition() ||
                       property->value()->IsAccessorFunctionDefinition(),
                   name != nullptr);
  }

  Expression* value = property->value();
  SetFunctionName(value, name, prefix);
}

void Parser::SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                             const AstRawString* name,
                                             const AstRawString* prefix) {
  // Ignore "__proto__" as a name when it's being used to set the [[Prototype]]
  // of an object literal.
  // See ES #sec-__proto__-property-names-in-object-initializers.
  if (property->IsPrototype()) return;

  DCHECK(!property->value()->IsAnonymousFunctionDefinition() ||
         property->kind() == ObjectLiteralProperty::COMPUTED);

  SetFunctionNameFromPropertyName(static_cast<LiteralProperty*>(property), name,
                                  prefix);
}

void Parser::SetFunctionNameFromIdentifierRef(Expression* value,
                                              Expression* identifier) {
  if (!identifier->IsVariableProxy()) return;
  SetFunctionName(value, identifier->AsVariableProxy()->raw_name());
}

void Parser::SetFunctionName(Expression* value, const AstRawString* name,
                             const AstRawString* prefix) {
  if (!value->IsAnonymousFunctionDefinition() &&
      !value->IsConciseMethodDefinition() &&
      !value->IsAccessorFunctionDefinition()) {
    return;
  }
  auto function = value->AsFunctionLiteral();
  if (value->IsClassLiteral()) {
    function = value->AsClassLiteral()->constructor();
  }
  if (function != nullptr) {
    AstConsString* cons_name = nullptr;
    if (name != nullptr) {
      if (prefix != nullptr) {
        cons_name = ast_value_factory()->NewConsString(prefix, name);
      } else {
        cons_name = ast_value_factory()->NewConsString(name);
      }
    } else {
      DCHECK_NULL(prefix);
    }
    function->set_raw_name(cons_name);
  }
}

Statement* Parser::CheckCallable(Variable* var, Expression* error, int pos) {
  const int nopos = kNoSourcePosition;
  Statement* validate_var;
  {
    Expression* type_of = factory()->NewUnaryOperation(
        Token::TYPEOF, factory()->NewVariableProxy(var), nopos);
    Expression* function_literal = factory()->NewStringLiteral(
        ast_value_factory()->function_string(), nopos);
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ_STRICT, type_of, function_literal, nopos);

    Statement* throw_call = factory()->NewExpressionStatement(error, pos);

    validate_var = factory()->NewIfStatement(
        condition, factory()->NewEmptyStatement(nopos), throw_call, nopos);
  }
  return validate_var;
}

void Parser::BuildIteratorClose(ZoneList<Statement*>* statements,
                                Variable* iterator, Variable* input,
                                Variable* var_output, IteratorType type) {
  //
  // This function adds four statements to [statements], corresponding to the
  // following code:
  //
  //   let iteratorReturn = iterator.return;
  //   if (IS_NULL_OR_UNDEFINED(iteratorReturn) {
  //     return {value: input, done: true};
  //   }
  //   output = %_Call(iteratorReturn, iterator, input);
  //   if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
  //

  const int nopos = kNoSourcePosition;

  // let iteratorReturn = iterator.return;
  Variable* var_return = var_output;  // Reusing the output variable.
  Statement* get_return;
  {
    Expression* iterator_proxy = factory()->NewVariableProxy(iterator);
    Expression* literal = factory()->NewStringLiteral(
        ast_value_factory()->return_string(), nopos);
    Expression* property =
        factory()->NewProperty(iterator_proxy, literal, nopos);
    Expression* return_proxy = factory()->NewVariableProxy(var_return);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, return_proxy, property, nopos);
    get_return = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (IS_NULL_OR_UNDEFINED(iteratorReturn) {
  //   return {value: input, done: true};
  // }
  Statement* check_return;
  {
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ, factory()->NewVariableProxy(var_return),
        factory()->NewNullLiteral(nopos), nopos);

    Expression* value = factory()->NewVariableProxy(input);

    Statement* return_input = BuildReturnStatement(value, nopos);

    check_return = factory()->NewIfStatement(
        condition, return_input, factory()->NewEmptyStatement(nopos), nopos);
  }

  // output = %_Call(iteratorReturn, iterator, input);
  Statement* call_return;
  {
    auto args = new (zone()) ZoneList<Expression*>(3, zone());
    args->Add(factory()->NewVariableProxy(var_return), zone());
    args->Add(factory()->NewVariableProxy(iterator), zone());
    args->Add(factory()->NewVariableProxy(input), zone());

    Expression* call =
        factory()->NewCallRuntime(Runtime::kInlineCall, args, nopos);
    if (type == IteratorType::kAsync) {
      function_state_->AddSuspend();
      call = factory()->NewAwait(call, nopos);
    }
    Expression* output_proxy = factory()->NewVariableProxy(var_output);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, output_proxy, call, nopos);
    call_return = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_RECEIVER(output)) %ThrowIteratorResultNotAnObject(output);
  Statement* validate_output;
  {
    Expression* is_receiver_call;
    {
      auto args = new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(factory()->NewVariableProxy(var_output), zone());
      is_receiver_call =
          factory()->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      auto args = new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(factory()->NewVariableProxy(var_output), zone());
      Expression* call = factory()->NewCallRuntime(
          Runtime::kThrowIteratorResultNotAnObject, args, nopos);
      throw_call = factory()->NewExpressionStatement(call, nopos);
    }

    validate_output = factory()->NewIfStatement(
        is_receiver_call, factory()->NewEmptyStatement(nopos), throw_call,
        nopos);
  }

  statements->Add(get_return, zone());
  statements->Add(check_return, zone());
  statements->Add(call_return, zone());
  statements->Add(validate_output, zone());
}

void Parser::FinalizeIteratorUse(Variable* completion, Expression* condition,
                                 Variable* iter, Block* iterator_use,
                                 Block* target, IteratorType type) {
  //
  // This function adds two statements to [target], corresponding to the
  // following code:
  //
  //   completion = kNormalCompletion;
  //   try {
  //     try {
  //       iterator_use
  //     } catch(e) {
  //       if (completion === kAbruptCompletion) completion = kThrowCompletion;
  //       %ReThrow(e);
  //     }
  //   } finally {
  //     if (condition) {
  //       #BuildIteratorCloseForCompletion(iter, completion)
  //     }
  //   }
  //

  const int nopos = kNoSourcePosition;

  // completion = kNormalCompletion;
  Statement* initialize_completion;
  {
    Expression* proxy = factory()->NewVariableProxy(completion);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, proxy,
        factory()->NewSmiLiteral(Parser::kNormalCompletion, nopos), nopos);
    initialize_completion =
        factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (completion === kAbruptCompletion) completion = kThrowCompletion;
  Statement* set_completion_throw;
  {
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ_STRICT, factory()->NewVariableProxy(completion),
        factory()->NewSmiLiteral(Parser::kAbruptCompletion, nopos), nopos);

    Expression* proxy = factory()->NewVariableProxy(completion);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, proxy,
        factory()->NewSmiLiteral(Parser::kThrowCompletion, nopos), nopos);
    Statement* statement = factory()->NewExpressionStatement(assignment, nopos);
    set_completion_throw = factory()->NewIfStatement(
        condition, statement, factory()->NewEmptyStatement(nopos), nopos);
  }

  // if (condition) {
  //   #BuildIteratorCloseForCompletion(iter, completion)
  // }
  Block* maybe_close;
  {
    Block* block = factory()->NewBlock(2, true);
    Expression* proxy = factory()->NewVariableProxy(completion);
    BuildIteratorCloseForCompletion(block->statements(), iter, proxy, type);
    DCHECK_EQ(block->statements()->length(), 2);

    maybe_close = IgnoreCompletion(factory()->NewIfStatement(
        condition, block, factory()->NewEmptyStatement(nopos), nopos));
  }

  // try { #try_block }
  // catch(e) {
  //   #set_completion_throw;
  //   %ReThrow(e);
  // }
  Statement* try_catch;
  {
    Scope* catch_scope = NewHiddenCatchScope();

    Statement* rethrow;
    // We use %ReThrow rather than the ordinary throw because we want to
    // preserve the original exception message.  This is also why we create a
    // TryCatchStatementForReThrow below (which does not clear the pending
    // message), rather than a TryCatchStatement.
    {
      auto args = new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(factory()->NewVariableProxy(catch_scope->catch_variable()),
                zone());
      rethrow = factory()->NewExpressionStatement(
          factory()->NewCallRuntime(Runtime::kReThrow, args, nopos), nopos);
    }

    Block* catch_block = factory()->NewBlock(2, false);
    catch_block->statements()->Add(set_completion_throw, zone());
    catch_block->statements()->Add(rethrow, zone());

    try_catch = factory()->NewTryCatchStatementForReThrow(
        iterator_use, catch_scope, catch_block, nopos);
  }

  // try { #try_catch } finally { #maybe_close }
  Statement* try_finally;
  {
    Block* try_block = factory()->NewBlock(1, false);
    try_block->statements()->Add(try_catch, zone());

    try_finally =
        factory()->NewTryFinallyStatement(try_block, maybe_close, nopos);
  }

  target->statements()->Add(initialize_completion, zone());
  target->statements()->Add(try_finally, zone());
}

void Parser::BuildIteratorCloseForCompletion(ZoneList<Statement*>* statements,
                                             Variable* iterator,
                                             Expression* completion,
                                             IteratorType type) {
  //
  // This function adds two statements to [statements], corresponding to the
  // following code:
  //
  //   let iteratorReturn = iterator.return;
  //   if (!IS_NULL_OR_UNDEFINED(iteratorReturn)) {
  //     if (completion === kThrowCompletion) {
  //       if (!IS_CALLABLE(iteratorReturn)) {
  //         throw MakeTypeError(kReturnMethodNotCallable);
  //       }
  //       [if (IteratorType == kAsync)]
  //           try { Await(%_Call(iteratorReturn, iterator) } catch (_) { }
  //       [else]
  //           try { %_Call(iteratorReturn, iterator) } catch (_) { }
  //       [endif]
  //     } else {
  //       [if (IteratorType == kAsync)]
  //           let output = Await(%_Call(iteratorReturn, iterator));
  //       [else]
  //           let output = %_Call(iteratorReturn, iterator);
  //       [endif]
  //       if (!IS_RECEIVER(output)) {
  //         %ThrowIterResultNotAnObject(output);
  //       }
  //     }
  //   }
  //

  const int nopos = kNoSourcePosition;
  // let iteratorReturn = iterator.return;
  Variable* var_return = NewTemporary(ast_value_factory()->empty_string());
  Statement* get_return;
  {
    Expression* iterator_proxy = factory()->NewVariableProxy(iterator);
    Expression* literal = factory()->NewStringLiteral(
        ast_value_factory()->return_string(), nopos);
    Expression* property =
        factory()->NewProperty(iterator_proxy, literal, nopos);
    Expression* return_proxy = factory()->NewVariableProxy(var_return);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, return_proxy, property, nopos);
    get_return = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_CALLABLE(iteratorReturn)) {
  //   throw MakeTypeError(kReturnMethodNotCallable);
  // }
  Statement* check_return_callable;
  {
    Expression* throw_expr =
        NewThrowTypeError(MessageTemplate::kReturnMethodNotCallable,
                          ast_value_factory()->empty_string(), nopos);
    check_return_callable = CheckCallable(var_return, throw_expr, nopos);
  }

  // try { %_Call(iteratorReturn, iterator) } catch (_) { }
  Statement* try_call_return;
  {
    auto args = new (zone()) ZoneList<Expression*>(2, zone());
    args->Add(factory()->NewVariableProxy(var_return), zone());
    args->Add(factory()->NewVariableProxy(iterator), zone());

    Expression* call =
        factory()->NewCallRuntime(Runtime::kInlineCall, args, nopos);

    if (type == IteratorType::kAsync) {
      function_state_->AddSuspend();
      call = factory()->NewAwait(call, nopos);
    }

    Block* try_block = factory()->NewBlock(1, false);
    try_block->statements()->Add(factory()->NewExpressionStatement(call, nopos),
                                 zone());

    Block* catch_block = factory()->NewBlock(0, false);
    try_call_return =
        factory()->NewTryCatchStatement(try_block, nullptr, catch_block, nopos);
  }

  // let output = %_Call(iteratorReturn, iterator);
  // if (!IS_RECEIVER(output)) {
  //   %ThrowIteratorResultNotAnObject(output);
  // }
  Block* validate_return;
  {
    Variable* var_output = NewTemporary(ast_value_factory()->empty_string());
    Statement* call_return;
    {
      auto args = new (zone()) ZoneList<Expression*>(2, zone());
      args->Add(factory()->NewVariableProxy(var_return), zone());
      args->Add(factory()->NewVariableProxy(iterator), zone());
      Expression* call =
          factory()->NewCallRuntime(Runtime::kInlineCall, args, nopos);
      if (type == IteratorType::kAsync) {
        function_state_->AddSuspend();
        call = factory()->NewAwait(call, nopos);
      }

      Expression* output_proxy = factory()->NewVariableProxy(var_output);
      Expression* assignment =
          factory()->NewAssignment(Token::ASSIGN, output_proxy, call, nopos);
      call_return = factory()->NewExpressionStatement(assignment, nopos);
    }

    Expression* is_receiver_call;
    {
      auto args = new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(factory()->NewVariableProxy(var_output), zone());
      is_receiver_call =
          factory()->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      auto args = new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(factory()->NewVariableProxy(var_output), zone());
      Expression* call = factory()->NewCallRuntime(
          Runtime::kThrowIteratorResultNotAnObject, args, nopos);
      throw_call = factory()->NewExpressionStatement(call, nopos);
    }

    Statement* check_return = factory()->NewIfStatement(
        is_receiver_call, factory()->NewEmptyStatement(nopos), throw_call,
        nopos);

    validate_return = factory()->NewBlock(2, false);
    validate_return->statements()->Add(call_return, zone());
    validate_return->statements()->Add(check_return, zone());
  }

  // if (completion === kThrowCompletion) {
  //   #check_return_callable;
  //   #try_call_return;
  // } else {
  //   #validate_return;
  // }
  Statement* call_return_carefully;
  {
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ_STRICT, completion,
        factory()->NewSmiLiteral(Parser::kThrowCompletion, nopos), nopos);

    Block* then_block = factory()->NewBlock(2, false);
    then_block->statements()->Add(check_return_callable, zone());
    then_block->statements()->Add(try_call_return, zone());

    call_return_carefully = factory()->NewIfStatement(condition, then_block,
                                                      validate_return, nopos);
  }

  // if (!IS_NULL_OR_UNDEFINED(iteratorReturn)) { ... }
  Statement* maybe_call_return;
  {
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ, factory()->NewVariableProxy(var_return),
        factory()->NewNullLiteral(nopos), nopos);

    maybe_call_return = factory()->NewIfStatement(
        condition, factory()->NewEmptyStatement(nopos), call_return_carefully,
        nopos);
  }

  statements->Add(get_return, zone());
  statements->Add(maybe_call_return, zone());
}

Statement* Parser::FinalizeForOfStatement(ForOfStatement* loop,
                                          Variable* var_completion,
                                          IteratorType type, int pos) {
  //
  // This function replaces the loop with the following wrapping:
  //
  //   completion = kNormalCompletion;
  //   try {
  //     try {
  //       #loop;
  //     } catch(e) {
  //       if (completion === kAbruptCompletion) completion = kThrowCompletion;
  //       %ReThrow(e);
  //     }
  //   } finally {
  //     if (!(completion === kNormalCompletion)) {
  //       #BuildIteratorCloseForCompletion(#iterator, completion)
  //     }
  //   }
  //
  // Note that the loop's body and its assign_each already contain appropriate
  // assignments to completion (see InitializeForOfStatement).
  //

  const int nopos = kNoSourcePosition;

  // !(completion === kNormalCompletion)
  Expression* closing_condition;
  {
    Expression* cmp = factory()->NewCompareOperation(
        Token::EQ_STRICT, factory()->NewVariableProxy(var_completion),
        factory()->NewSmiLiteral(Parser::kNormalCompletion, nopos), nopos);
    closing_condition = factory()->NewUnaryOperation(Token::NOT, cmp, nopos);
  }

  Block* final_loop = factory()->NewBlock(2, false);
  {
    Block* try_block = factory()->NewBlock(1, false);
    try_block->statements()->Add(loop, zone());

    FinalizeIteratorUse(var_completion, closing_condition, loop->iterator(),
                        try_block, final_loop, type);
  }

  return final_loop;
}

#undef CHECK_OK
#undef CHECK_OK_VOID
#undef CHECK_FAILED

}  // namespace internal
}  // namespace v8
