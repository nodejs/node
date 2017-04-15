// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parser.h"

#include <memory>

#include "src/api.h"
#include "src/ast/ast-expression-rewriter.h"
#include "src/ast/ast-function-literal-id-reindexer.h"
#include "src/ast/ast-literal-reindexer.h"
#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/ast.h"
#include "src/bailout-reason.h"
#include "src/base/platform/platform.h"
#include "src/char-predicates-inl.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/parsing/duplicate-finder.h"
#include "src/parsing/parameter-initializer-rewriter.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/rewriter.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/runtime/runtime.h"
#include "src/string-stream.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

ScriptData::ScriptData(const byte* data, int length)
    : owns_data_(false), rejected_(false), data_(data), length_(length) {
  if (!IsAligned(reinterpret_cast<intptr_t>(data), kPointerAlignment)) {
    byte* copy = NewArray<byte>(length);
    DCHECK(IsAligned(reinterpret_cast<intptr_t>(copy), kPointerAlignment));
    CopyBytes(copy, data, length);
    data_ = copy;
    AcquireDataOwnership();
  }
}

FunctionEntry ParseData::GetFunctionEntry(int start) {
  // The current pre-data entry must be a FunctionEntry with the given
  // start position.
  if ((function_index_ + FunctionEntry::kSize <= Length()) &&
      (static_cast<int>(Data()[function_index_]) == start)) {
    int index = function_index_;
    function_index_ += FunctionEntry::kSize;
    Vector<unsigned> subvector(&(Data()[index]), FunctionEntry::kSize);
    return FunctionEntry(subvector);
  }
  return FunctionEntry();
}


int ParseData::FunctionCount() {
  int functions_size = FunctionsSize();
  if (functions_size < 0) return 0;
  if (functions_size % FunctionEntry::kSize != 0) return 0;
  return functions_size / FunctionEntry::kSize;
}


bool ParseData::IsSane() {
  if (!IsAligned(script_data_->length(), sizeof(unsigned))) return false;
  // Check that the header data is valid and doesn't specify
  // point to positions outside the store.
  int data_length = Length();
  if (data_length < PreparseDataConstants::kHeaderSize) return false;
  if (Magic() != PreparseDataConstants::kMagicNumber) return false;
  if (Version() != PreparseDataConstants::kCurrentVersion) return false;
  // Check that the space allocated for function entries is sane.
  int functions_size = FunctionsSize();
  if (functions_size < 0) return false;
  if (functions_size % FunctionEntry::kSize != 0) return false;
  // Check that the total size has room for header and function entries.
  int minimum_size =
      PreparseDataConstants::kHeaderSize + functions_size;
  if (data_length < minimum_size) return false;
  return true;
}


void ParseData::Initialize() {
  // Prepares state for use.
  int data_length = Length();
  if (data_length >= PreparseDataConstants::kHeaderSize) {
    function_index_ = PreparseDataConstants::kHeaderSize;
  }
}


unsigned ParseData::Magic() {
  return Data()[PreparseDataConstants::kMagicOffset];
}


unsigned ParseData::Version() {
  return Data()[PreparseDataConstants::kVersionOffset];
}


int ParseData::FunctionsSize() {
  return static_cast<int>(Data()[PreparseDataConstants::kFunctionsSizeOffset]);
}

// Helper for putting parts of the parse results into a temporary zone when
// parsing inner function bodies.
class DiscardableZoneScope {
 public:
  DiscardableZoneScope(Parser* parser, Zone* temp_zone, bool use_temp_zone)
      : ast_node_factory_scope_(parser->factory(), temp_zone, use_temp_zone),
        fni_(parser->ast_value_factory_, temp_zone),
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
      if (parser_->reusable_preparser_ != nullptr) {
        parser_->reusable_preparser_->zone_ = temp_zone;
        parser_->reusable_preparser_->factory()->set_zone(temp_zone);
      }
    }
  }
  void Reset() {
    parser_->fni_ = prev_fni_;
    parser_->zone_ = prev_zone_;
    parser_->allow_lazy_ = prev_allow_lazy_;
    parser_->temp_zoned_ = prev_temp_zoned_;
    if (parser_->reusable_preparser_ != nullptr) {
      parser_->reusable_preparser_->zone_ = prev_zone_;
      parser_->reusable_preparser_->factory()->set_zone(prev_zone_);
    }
    ast_node_factory_scope_.Reset();
  }
  ~DiscardableZoneScope() { Reset(); }

 private:
  AstNodeFactory::BodyScope ast_node_factory_scope_;
  FuncNameInferrer fni_;
  Parser* parser_;
  FuncNameInferrer* prev_fni_;
  Zone* prev_zone_;
  bool prev_allow_lazy_;
  bool prev_temp_zoned_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableZoneScope);
};

void Parser::SetCachedData(ParseInfo* info) {
  DCHECK_NULL(cached_parse_data_);
  if (consume_cached_parse_data()) {
    if (allow_lazy_) {
      cached_parse_data_ = ParseData::FromCachedData(*info->cached_data());
      if (cached_parse_data_ != nullptr) return;
    }
    compile_options_ = ScriptCompiler::kNoCompileOptions;
  }
}

FunctionLiteral* Parser::DefaultConstructor(const AstRawString* name,
                                            bool call_super, int pos,
                                            int end_pos) {
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  const int parameter_count = 0;
  if (name == nullptr) name = ast_value_factory()->empty_string();

  FunctionKind kind = call_super ? FunctionKind::kDefaultDerivedConstructor
                                 : FunctionKind::kDefaultBaseConstructor;
  DeclarationScope* function_scope = NewFunctionScope(kind);
  SetLanguageMode(function_scope, STRICT);
  // Set start and end position to the same value
  function_scope->set_start_position(pos);
  function_scope->set_end_position(pos);
  ZoneList<Statement*>* body = NULL;

  {
    FunctionState function_state(&function_state_, &scope_state_,
                                 function_scope);

    body = new (zone()) ZoneList<Statement*>(call_super ? 2 : 1, zone());
    if (call_super) {
      // Create a SuperCallReference and handle in BytecodeGenerator.
      auto constructor_args_name = ast_value_factory()->empty_string();
      bool is_duplicate;
      bool is_rest = true;
      bool is_optional = false;
      Variable* constructor_args = function_scope->DeclareParameter(
          constructor_args_name, TEMPORARY, is_optional, is_rest, &is_duplicate,
          ast_value_factory());

      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(1, zone());
      Spread* spread_args = factory()->NewSpread(
          factory()->NewVariableProxy(constructor_args), pos, pos);

      args->Add(spread_args, zone());
      Expression* super_call_ref = NewSuperCallReference(pos);
      Expression* call = factory()->NewCall(super_call_ref, args, pos);
      body->Add(factory()->NewReturnStatement(call, pos), zone());
    }

    materialized_literal_count = function_state.materialized_literal_count();
    expected_property_count = function_state.expected_property_count();
  }

  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      name, function_scope, body, materialized_literal_count,
      expected_property_count, parameter_count, parameter_count,
      FunctionLiteral::kNoDuplicateParameters,
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
  if ((*x)->AsLiteral() && (*x)->AsLiteral()->raw_value()->IsNumber() &&
      y->AsLiteral() && y->AsLiteral()->raw_value()->IsNumber()) {
    double x_val = (*x)->AsLiteral()->raw_value()->AsNumber();
    double y_val = y->AsLiteral()->raw_value()->AsNumber();
    bool x_has_dot = (*x)->AsLiteral()->raw_value()->ContainsDot();
    bool y_has_dot = y->AsLiteral()->raw_value()->ContainsDot();
    bool has_dot = x_has_dot || y_has_dot;
    switch (op) {
      case Token::ADD:
        *x = factory()->NewNumberLiteral(x_val + y_val, pos, has_dot);
        return true;
      case Token::SUB:
        *x = factory()->NewNumberLiteral(x_val - y_val, pos, has_dot);
        return true;
      case Token::MUL:
        *x = factory()->NewNumberLiteral(x_val * y_val, pos, has_dot);
        return true;
      case Token::DIV:
        *x = factory()->NewNumberLiteral(x_val / y_val, pos, has_dot);
        return true;
      case Token::BIT_OR: {
        int value = DoubleToInt32(x_val) | DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::BIT_AND: {
        int value = DoubleToInt32(x_val) & DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::BIT_XOR: {
        int value = DoubleToInt32(x_val) ^ DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::SHL: {
        int value = DoubleToInt32(x_val) << (DoubleToInt32(y_val) & 0x1f);
        *x = factory()->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::SHR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1f;
        uint32_t value = DoubleToUint32(x_val) >> shift;
        *x = factory()->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::SAR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1f;
        int value = ArithmeticShiftRight(DoubleToInt32(x_val), shift);
        *x = factory()->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::EXP: {
        double value = Pow(x_val, y_val);
        int int_value = static_cast<int>(value);
        *x = factory()->NewNumberLiteral(
            int_value == value && value != -0.0 ? int_value : value, pos,
            has_dot);
        return true;
      }
      default:
        break;
    }
  }
  return false;
}

Expression* Parser::BuildUnaryExpression(Expression* expression,
                                         Token::Value op, int pos) {
  DCHECK(expression != NULL);
  if (expression->IsLiteral()) {
    const AstValue* literal = expression->AsLiteral()->raw_value();
    if (op == Token::NOT) {
      // Convert the literal to a boolean condition and negate it.
      bool condition = literal->BooleanValue();
      return factory()->NewBooleanLiteral(!condition, pos);
    } else if (literal->IsNumber()) {
      // Compute some expressions involving only number literals.
      double value = literal->AsNumber();
      bool has_dot = literal->ContainsDot();
      switch (op) {
        case Token::ADD:
          return expression;
        case Token::SUB:
          return factory()->NewNumberLiteral(-value, pos, has_dot);
        case Token::BIT_NOT:
          return factory()->NewNumberLiteral(~DoubleToInt32(value), pos,
                                             has_dot);
        default:
          break;
      }
    }
  }
  // Desugar '+foo' => 'foo*1'
  if (op == Token::ADD) {
    return factory()->NewBinaryOperation(
        Token::MUL, expression, factory()->NewNumberLiteral(1, pos, true), pos);
  }
  // The same idea for '-foo' => 'foo*(-1)'.
  if (op == Token::SUB) {
    return factory()->NewBinaryOperation(
        Token::MUL, expression, factory()->NewNumberLiteral(-1, pos), pos);
  }
  // ...and one more time for '~foo' => 'foo^(~0)'.
  if (op == Token::BIT_NOT) {
    return factory()->NewBinaryOperation(
        Token::BIT_XOR, expression, factory()->NewNumberLiteral(~0, pos), pos);
  }
  return factory()->NewUnaryOperation(op, expression, pos);
}

Expression* Parser::BuildIteratorResult(Expression* value, bool done) {
  int pos = kNoSourcePosition;

  if (value == nullptr) value = factory()->NewUndefinedLiteral(pos);

  auto args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(value, zone());
  args->Add(factory()->NewBooleanLiteral(done, pos), zone());

  return factory()->NewCallRuntime(Runtime::kInlineCreateIterResultObject, args,
                                   pos);
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
  Expression* home_object_symbol_literal =
      factory()->NewSymbolLiteral("home_object_symbol", kNoSourcePosition);
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

Expression* Parser::FunctionSentExpression(int pos) {
  // We desugar function.sent into %_GeneratorGetInputOrDebugPos(generator).
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(1, zone());
  VariableProxy* generator =
      factory()->NewVariableProxy(function_state_->generator_object_variable());
  args->Add(generator, zone());
  return factory()->NewCallRuntime(Runtime::kInlineGeneratorGetInputOrDebugPos,
                                   args, pos);
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
      bool has_dot = scanner()->ContainsDot();
      double value = scanner()->DoubleValue();
      return factory()->NewNumberLiteral(value, pos, has_dot);
    }
    default:
      DCHECK(false);
  }
  return NULL;
}

void Parser::MarkTailPosition(Expression* expression) {
  expression->MarkTail();
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
                         info->extension(), info->ast_value_factory(),
                         info->isolate()->counters()->runtime_call_stats(),
                         true),
      scanner_(info->unicode_cache()),
      reusable_preparser_(nullptr),
      original_scope_(nullptr),
      mode_(PARSE_EAGERLY),  // Lazy mode must be set explicitly.
      target_stack_(nullptr),
      compile_options_(info->compile_options()),
      cached_parse_data_(nullptr),
      total_preparse_skipped_(0),
      temp_zoned_(false),
      log_(nullptr) {
  // Even though we were passed ParseInfo, we should not store it in
  // Parser - this makes sure that Isolate is not accidentally accessed via
  // ParseInfo during background parsing.
  DCHECK(!info->script().is_null() || info->source_stream() != nullptr ||
         info->character_stream() != nullptr);
  // Determine if functions can be lazily compiled. This is necessary to
  // allow some of our builtin JS files to be lazily compiled. These
  // builtins cannot be handled lazily by the parser, since we have to know
  // if a function uses the special natives syntax, which is something the
  // parser records.
  // If the debugger requests compilation for break points, we cannot be
  // aggressive about lazy compilation, because it might trigger compilation
  // of functions without an outer context when setting a breakpoint through
  // Debug::FindSharedFunctionInfoInScript
  bool can_compile_lazily = FLAG_lazy && !info->is_debug();

  // Consider compiling eagerly when targeting the code cache.
  can_compile_lazily &= !(FLAG_serialize_eager && info->will_serialize());

  set_default_eager_compile_hint(can_compile_lazily
                                     ? FunctionLiteral::kShouldLazyCompile
                                     : FunctionLiteral::kShouldEagerCompile);
  allow_lazy_ = FLAG_lazy && info->allow_lazy_parsing() && !info->is_native() &&
                info->extension() == nullptr && can_compile_lazily;
  set_allow_natives(FLAG_allow_natives_syntax || info->is_native());
  set_allow_tailcalls(FLAG_harmony_tailcalls && !info->is_native() &&
                      info->isolate()->is_tail_call_elimination_enabled());
  set_allow_harmony_do_expressions(FLAG_harmony_do_expressions);
  set_allow_harmony_function_sent(FLAG_harmony_function_sent);
  set_allow_harmony_restrictive_generators(FLAG_harmony_restrictive_generators);
  set_allow_harmony_trailing_commas(FLAG_harmony_trailing_commas);
  set_allow_harmony_class_fields(FLAG_harmony_class_fields);
  set_allow_harmony_object_spread(FLAG_harmony_object_spread);
  for (int feature = 0; feature < v8::Isolate::kUseCounterFeatureCount;
       ++feature) {
    use_counts_[feature] = 0;
  }
  if (info->ast_value_factory() == NULL) {
    // info takes ownership of AstValueFactory.
    info->set_ast_value_factory(new AstValueFactory(
        zone(), info->isolate()->ast_string_constants(), info->hash_seed()));
    info->set_ast_value_factory_owned();
    ast_value_factory_ = info->ast_value_factory();
    ast_node_factory_.set_ast_value_factory(ast_value_factory_);
  }
}

void Parser::DeserializeScopeChain(
    ParseInfo* info, MaybeHandle<ScopeInfo> maybe_outer_scope_info) {
  DCHECK(ThreadId::Current().Equals(info->isolate()->thread_id()));
  // TODO(wingo): Add an outer SCRIPT_SCOPE corresponding to the native
  // context, which will have the "this" binding for script scopes.
  DeclarationScope* script_scope = NewScriptScope();
  info->set_script_scope(script_scope);
  Scope* scope = script_scope;
  Handle<ScopeInfo> outer_scope_info;
  if (maybe_outer_scope_info.ToHandle(&outer_scope_info)) {
    scope = Scope::DeserializeScopeChain(
        info->isolate(), zone(), *outer_scope_info, script_scope,
        ast_value_factory(), Scope::DeserializationMode::kScopesOnly);
    DCHECK(!info->is_module() || scope->is_module_scope());
  }
  original_scope_ = scope;
}

FunctionLiteral* Parser::ParseProgram(Isolate* isolate, ParseInfo* info) {
  // TODO(bmeurer): We temporarily need to pass allow_nesting = true here,
  // see comment for HistogramTimerScope class.

  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);
  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_, info->is_eval() ? &RuntimeCallStats::ParseEval
                                           : &RuntimeCallStats::ParseProgram);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.ParseProgram");
  Handle<String> source(String::cast(info->script()->source()));
  isolate->counters()->total_parse_size()->Increment(source->length());
  base::ElapsedTimer timer;
  if (FLAG_trace_parse) {
    timer.Start();
  }
  fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());

  // Initialize parser state.
  ParserLogger logger;

  if (produce_cached_parse_data()) {
    if (allow_lazy_) {
      log_ = &logger;
    } else {
      compile_options_ = ScriptCompiler::kNoCompileOptions;
    }
  } else if (consume_cached_parse_data()) {
    cached_parse_data_->Initialize();
  }

  DeserializeScopeChain(info, info->maybe_outer_scope_info());

  source = String::Flatten(source);
  FunctionLiteral* result;

  {
    std::unique_ptr<Utf16CharacterStream> stream(ScannerStream::For(source));
    scanner_.Initialize(stream.get());
    result = DoParseProgram(info);
  }
  if (result != NULL) {
    DCHECK_EQ(scanner_.peek_location().beg_pos, source->length());
  }
  HandleSourceURLComments(isolate, info->script());

  if (FLAG_trace_parse && result != nullptr) {
    double ms = timer.Elapsed().InMillisecondsF();
    if (info->is_eval()) {
      PrintF("[parsing eval");
    } else if (info->script()->name()->IsString()) {
      String* name = String::cast(info->script()->name());
      std::unique_ptr<char[]> name_chars = name->ToCString();
      PrintF("[parsing script: %s", name_chars.get());
    } else {
      PrintF("[parsing script");
    }
    PrintF(" - took %0.3f ms]\n", ms);
  }
  if (produce_cached_parse_data() && result != nullptr) {
    *info->cached_data() = logger.GetScriptData();
  }
  log_ = nullptr;
  return result;
}


FunctionLiteral* Parser::DoParseProgram(ParseInfo* info) {
  // Note that this function can be called from the main thread or from a
  // background thread. We should not access anything Isolate / heap dependent
  // via ParseInfo, and also not pass it forward.
  DCHECK_NULL(scope_state_);
  DCHECK_NULL(target_stack_);

  ParsingModeScope mode(this, allow_lazy_ ? PARSE_LAZILY : PARSE_EAGERLY);
  ResetFunctionLiteralId();
  DCHECK(info->function_literal_id() == FunctionLiteral::kIdTypeTopLevel ||
         info->function_literal_id() == FunctionLiteral::kIdTypeInvalid);

  FunctionLiteral* result = NULL;
  {
    Scope* outer = original_scope_;
    DCHECK_NOT_NULL(outer);
    parsing_module_ = info->is_module();
    if (info->is_eval()) {
      outer = NewEvalScope(outer);
    } else if (parsing_module_) {
      DCHECK_EQ(outer, info->script_scope());
      outer = NewModuleScope(info->script_scope());
    }

    DeclarationScope* scope = outer->AsDeclarationScope();

    scope->set_start_position(0);

    FunctionState function_state(&function_state_, &scope_state_, scope);

    ZoneList<Statement*>* body = new(zone()) ZoneList<Statement*>(16, zone());
    bool ok = true;
    int beg_pos = scanner()->location().beg_pos;
    if (parsing_module_) {
      // Declare the special module parameter.
      auto name = ast_value_factory()->empty_string();
      bool is_duplicate;
      bool is_rest = false;
      bool is_optional = false;
      auto var = scope->DeclareParameter(name, VAR, is_optional, is_rest,
                                         &is_duplicate, ast_value_factory());
      DCHECK(!is_duplicate);
      var->AllocateTo(VariableLocation::PARAMETER, 0);

      PrepareGeneratorVariables();
      Expression* initial_yield =
          BuildInitialYield(kNoSourcePosition, kGeneratorFunction);
      body->Add(
          factory()->NewExpressionStatement(initial_yield, kNoSourcePosition),
          zone());

      ParseModuleItemList(body, &ok);
      ok = ok &&
           module()->Validate(this->scope()->AsModuleScope(),
                              &pending_error_handler_, zone());
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
          scope, body, function_state.materialized_literal_count(),
          function_state.expected_property_count(), parameter_count);
    }
  }

  info->set_max_function_literal_id(GetLastFunctionLiteralId());

  // Make sure the target stack is empty.
  DCHECK(target_stack_ == NULL);

  return result;
}

FunctionLiteral* Parser::ParseFunction(Isolate* isolate, ParseInfo* info) {
  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);
  RuntimeCallTimerScope runtime_timer(runtime_call_stats_,
                                      &RuntimeCallStats::ParseFunction);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.ParseFunction");
  Handle<String> source(String::cast(info->script()->source()));
  isolate->counters()->total_parse_size()->Increment(source->length());
  base::ElapsedTimer timer;
  if (FLAG_trace_parse) {
    timer.Start();
  }
  Handle<SharedFunctionInfo> shared_info = info->shared_info();
  DeserializeScopeChain(info, info->maybe_outer_scope_info());
  if (info->asm_function_scope()) {
    original_scope_ = info->asm_function_scope();
    factory()->set_zone(info->zone());
  } else {
    DCHECK_EQ(factory()->zone(), info->zone());
  }

  // Initialize parser state.
  source = String::Flatten(source);
  FunctionLiteral* result;
  {
    std::unique_ptr<Utf16CharacterStream> stream(ScannerStream::For(
        source, shared_info->start_position(), shared_info->end_position()));
    Handle<String> name(String::cast(shared_info->name()));
    result = DoParseFunction(info, ast_value_factory()->GetString(name),
                             stream.get());
    if (result != nullptr) {
      Handle<String> inferred_name(shared_info->inferred_name());
      result->set_inferred_name(inferred_name);
    }
  }

  if (FLAG_trace_parse && result != NULL) {
    double ms = timer.Elapsed().InMillisecondsF();
    // We need to make sure that the debug-name is available.
    ast_value_factory()->Internalize(isolate);
    std::unique_ptr<char[]> name_chars = result->debug_name()->ToCString();
    PrintF("[parsing function: %s - took %0.3f ms]\n", name_chars.get(), ms);
  }
  return result;
}

static FunctionLiteral::FunctionType ComputeFunctionType(ParseInfo* info) {
  if (info->is_declaration()) {
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
                                         const AstRawString* raw_name,
                                         Utf16CharacterStream* source) {
  scanner_.Initialize(source);
  DCHECK_NULL(scope_state_);
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
    FunctionState function_state(&function_state_, &scope_state_,
                                 outer_function);
    BlockState block_state(&scope_state_, outer);
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

      // These two bits only need to be explicitly set because we're
      // not passing the ScopeInfo to the Scope constructor.
      // TODO(adamk): Remove these calls once the above NewScope call
      // passes the ScopeInfo.
      if (info->calls_eval()) {
        scope->RecordEvalCall();
      }
      SetLanguageMode(scope, info->language_mode());

      scope->set_start_position(info->start_position());
      ExpressionClassifier formals_classifier(this);
      ParserFormalParameters formals(scope);
      Checkpoint checkpoint(this);
      {
        // Parsing patterns as variable reference expression creates
        // NewUnresolved references in current scope. Entrer arrow function
        // scope for formal parameter parsing.
        BlockState block_state(&scope_state_, scope);
        if (Check(Token::LPAREN)) {
          // '(' StrictFormalParameters ')'
          ParseFormalParameterList(&formals, &ok);
          if (ok) ok = Check(Token::RPAREN);
        } else {
          // BindingIdentifier
          ParseFormalParameter(&formals, &ok);
          if (ok) DeclareFormalParameters(formals.scope, formals.params);
        }
      }

      if (ok) {
        checkpoint.Restore(&formals.materialized_literals_count);
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
        Expression* expression = ParseArrowFunctionLiteral(true, formals, &ok);
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
      result = ParseFunctionLiteral(
          raw_name, Scanner::Location::invalid(), kSkipFunctionNameCheck, kind,
          kNoSourcePosition, function_type, info->language_mode(), &ok);
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

  switch (peek()) {
    case Token::IMPORT:
      ParseImportDeclaration(CHECK_OK);
      return factory()->NewEmptyStatement(kNoSourcePosition);
    case Token::EXPORT:
      return ParseExportDeclaration(ok);
    default:
      return ParseStatementListItem(ok);
  }
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
        !Token::IsIdentifier(name_tok, STRICT, false, parsing_module_)) {
      *reserved_loc = scanner()->location();
    }
    const AstRawString* local_name = ParseIdentifierName(CHECK_OK_VOID);
    const AstRawString* export_name = NULL;
    Scanner::Location location = scanner()->location();
    if (CheckContextualKeyword(CStrVector("as"))) {
      export_name = ParseIdentifierName(CHECK_OK_VOID);
      // Set the location to the whole "a as b" string, so that it makes sense
      // both for errors due to "a" and for errors due to "b".
      location.end_pos = scanner()->location().end_pos;
    }
    if (export_name == NULL) {
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
    if (CheckContextualKeyword(CStrVector("as"))) {
      local_name = ParseIdentifierName(CHECK_OK);
    }
    if (!Token::IsIdentifier(scanner()->current_token(), STRICT, false,
                             parsing_module_)) {
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
    const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK_VOID);
    ExpectSemicolon(CHECK_OK_VOID);
    module()->AddEmptyImport(module_specifier);
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
        ExpectContextualKeyword(CStrVector("as"), CHECK_OK_VOID);
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

  ExpectContextualKeyword(CStrVector("from"), CHECK_OK_VOID);
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
                            module_namespace_binding_loc, zone());
  }

  if (import_default_binding != nullptr) {
    module()->AddImport(ast_value_factory()->default_string(),
                        import_default_binding, module_specifier,
                        import_default_binding_loc, zone());
  }

  if (named_imports != nullptr) {
    if (named_imports->length() == 0) {
      module()->AddEmptyImport(module_specifier);
    } else {
      for (int i = 0; i < named_imports->length(); ++i) {
        const NamedImport* import = named_imports->at(i);
        module()->AddImport(import->import_name, import->local_name,
                            module_specifier, import->location, zone());
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
    /* falls through */

    default: {
      int pos = position();
      ExpressionClassifier classifier(this);
      Expression* value = ParseAssignmentExpression(true, CHECK_OK);
      RewriteNonPattern(CHECK_OK);
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
      result = factory()->NewExpressionStatement(assignment, kNoSourcePosition);

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
      ExpectContextualKeyword(CStrVector("from"), CHECK_OK);
      const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK);
      ExpectSemicolon(CHECK_OK);
      module()->AddStarExport(module_specifier, loc, zone());
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
      if (CheckContextualKeyword(CStrVector("from"))) {
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
        module()->AddEmptyImport(module_specifier);
      } else {
        for (int i = 0; i < length; ++i) {
          module()->AddExport(original_names[i], export_names[i],
                              module_specifier, export_locations[i], zone());
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
  Declaration* declaration =
      factory()->NewVariableDeclaration(proxy, this->scope(), pos);
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
      declaration, mode, init, allow_harmony_restrictive_generators(),
      &sloppy_mode_block_scope_function_redefinition, ok);
  if (!*ok) {
    // If we only have the start position of a proxy, we can't highlight the
    // whole variable name.  Pretend its length is 1 so that we highlight at
    // least the first character.
    Scanner::Location loc(declaration->proxy()->position(),
                          var_end_pos != kNoSourcePosition
                              ? var_end_pos
                              : declaration->proxy()->position() + 1);
    if (declaration_kind == DeclarationDescriptor::NORMAL) {
      ReportMessageAt(loc, MessageTemplate::kVarRedeclaration,
                      declaration->proxy()->raw_name());
    } else {
      ReportMessageAt(loc, MessageTemplate::kParamDupe);
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
  Block* result = factory()->NewBlock(
      NULL, 1, true, parsing_result->descriptor.declaration_pos);
  for (auto declaration : parsing_result->declarations) {
    PatternRewriter::DeclareAndInitializeVariables(
        this, result, &(parsing_result->descriptor), &declaration, names,
        CHECK_OK);
  }
  return result;
}

void Parser::DeclareAndInitializeVariables(
    Block* block, const DeclarationDescriptor* declaration_descriptor,
    const DeclarationParsingResult::Declaration* declaration,
    ZoneList<const AstRawString*>* names, bool* ok) {
  DCHECK_NOT_NULL(block);
  PatternRewriter::DeclareAndInitializeVariables(
      this, block, declaration_descriptor, declaration, names, ok);
}

Statement* Parser::DeclareFunction(const AstRawString* variable_name,
                                   FunctionLiteral* function, VariableMode mode,
                                   int pos, bool is_generator, bool is_async,
                                   bool is_sloppy_block_function,
                                   ZoneList<const AstRawString*>* names,
                                   bool* ok) {
  VariableProxy* proxy =
      factory()->NewVariableProxy(variable_name, NORMAL_VARIABLE);
  Declaration* declaration =
      factory()->NewFunctionDeclaration(proxy, function, scope(), pos);
  Declare(declaration, DeclarationDescriptor::NORMAL, mode, kCreatedInitialized,
          CHECK_OK);
  if (names) names->Add(variable_name, zone());
  if (is_sloppy_block_function) {
    SloppyBlockFunctionStatement* statement =
        factory()->NewSloppyBlockFunctionStatement();
    DeclarationScope* target_scope = GetDeclarationScope();
    target_scope->DeclareSloppyBlockFunction(variable_name, scope(), statement);
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
  Assignment* assignment = factory()->NewAssignment(Token::INIT, decl->proxy(),
                                                    value, class_token_pos);
  Statement* assignment_statement =
      factory()->NewExpressionStatement(assignment, kNoSourcePosition);
  if (names) names->Add(variable_name, zone());
  return assignment_statement;
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

Expression* Parser::RewriteReturn(Expression* return_value, int pos) {
  if (IsDerivedConstructor(function_state_->kind())) {
    // For subclass constructors we need to return this in case of undefined
    // return a Smi (transformed into an exception in the ConstructStub)
    // for a non object.
    //
    //   return expr;
    //
    // Is rewritten as:
    //
    //   return (temp = expr) === undefined ? this :
    //       %_IsJSReceiver(temp) ? temp : 1;

    // temp = expr
    Variable* temp = NewTemporary(ast_value_factory()->empty_string());
    Assignment* assign = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(temp), return_value, pos);

    // %_IsJSReceiver(temp)
    ZoneList<Expression*>* is_spec_object_args =
        new (zone()) ZoneList<Expression*>(1, zone());
    is_spec_object_args->Add(factory()->NewVariableProxy(temp), zone());
    Expression* is_spec_object_call = factory()->NewCallRuntime(
        Runtime::kInlineIsJSReceiver, is_spec_object_args, pos);

    // %_IsJSReceiver(temp) ? temp : 1;
    Expression* is_object_conditional = factory()->NewConditional(
        is_spec_object_call, factory()->NewVariableProxy(temp),
        factory()->NewSmiLiteral(1, pos), pos);

    // temp === undefined
    Expression* is_undefined = factory()->NewCompareOperation(
        Token::EQ_STRICT, assign,
        factory()->NewUndefinedLiteral(kNoSourcePosition), pos);

    // is_undefined ? this : is_object_conditional
    return_value = factory()->NewConditional(is_undefined, ThisExpression(pos),
                                             is_object_conditional, pos);
  }
  if (is_generator()) {
    return_value = BuildIteratorResult(return_value, true);
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

Statement* Parser::RewriteSwitchStatement(Expression* tag,
                                          SwitchStatement* switch_statement,
                                          ZoneList<CaseClause*>* cases,
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

  Block* switch_block = factory()->NewBlock(NULL, 2, false, kNoSourcePosition);

  Variable* tag_variable =
      NewTemporary(ast_value_factory()->dot_switch_tag_string());
  Assignment* tag_assign = factory()->NewAssignment(
      Token::ASSIGN, factory()->NewVariableProxy(tag_variable), tag,
      tag->position());
  Statement* tag_statement =
      factory()->NewExpressionStatement(tag_assign, kNoSourcePosition);
  switch_block->statements()->Add(tag_statement, zone());

  // make statement: undefined;
  // This is needed so the tag isn't returned as the value, in case the switch
  // statements don't have a value.
  switch_block->statements()->Add(
      factory()->NewExpressionStatement(
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition),
      zone());

  Expression* tag_read = factory()->NewVariableProxy(tag_variable);
  switch_statement->Initialize(tag_read, cases);
  Block* cases_block = factory()->NewBlock(NULL, 1, false, kNoSourcePosition);
  cases_block->statements()->Add(switch_statement, zone());
  cases_block->set_scope(scope);
  DCHECK_IMPLIES(scope != nullptr,
                 switch_statement->position() >= scope->start_position());
  DCHECK_IMPLIES(scope != nullptr,
                 switch_statement->position() < scope->end_position());
  switch_block->statements()->Add(cases_block, zone());
  return switch_block;
}

void Parser::RewriteCatchPattern(CatchInfo* catch_info, bool* ok) {
  if (catch_info->name == nullptr) {
    DCHECK_NOT_NULL(catch_info->pattern);
    catch_info->name = ast_value_factory()->dot_catch_string();
  }
  catch_info->variable = catch_info->scope->DeclareLocal(catch_info->name, VAR);
  if (catch_info->pattern != nullptr) {
    DeclarationDescriptor descriptor;
    descriptor.declaration_kind = DeclarationDescriptor::NORMAL;
    descriptor.scope = scope();
    descriptor.hoist_scope = nullptr;
    descriptor.mode = LET;
    descriptor.declaration_pos = catch_info->pattern->position();
    descriptor.initialization_pos = catch_info->pattern->position();

    // Initializer position for variables declared by the pattern.
    const int initializer_position = position();

    DeclarationParsingResult::Declaration decl(
        catch_info->pattern, initializer_position,
        factory()->NewVariableProxy(catch_info->variable));

    catch_info->init_block =
        factory()->NewBlock(nullptr, 8, true, kNoSourcePosition);
    PatternRewriter::DeclareAndInitializeVariables(
        this, catch_info->init_block, &descriptor, &decl,
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
                                       Block* finally_block,
                                       const CatchInfo& catch_info, int pos) {
  // Simplify the AST nodes by converting:
  //   'try B0 catch B1 finally B2'
  // to:
  //   'try { try B0 catch B1 } finally B2'

  if (catch_block != nullptr && finally_block != nullptr) {
    // If we have both, create an inner try/catch.
    DCHECK_NOT_NULL(catch_info.scope);
    DCHECK_NOT_NULL(catch_info.variable);
    TryCatchStatement* statement;
    statement = factory()->NewTryCatchStatement(try_block, catch_info.scope,
                                                catch_info.variable,
                                                catch_block, kNoSourcePosition);

    try_block = factory()->NewBlock(nullptr, 1, false, kNoSourcePosition);
    try_block->statements()->Add(statement, zone());
    catch_block = nullptr;  // Clear to indicate it's been handled.
  }

  if (catch_block != nullptr) {
    // For a try-catch construct append return expressions from the catch block
    // to the list of return expressions.
    function_state_->tail_call_expressions().Append(
        catch_info.tail_call_expressions);

    DCHECK_NULL(finally_block);
    DCHECK_NOT_NULL(catch_info.scope);
    DCHECK_NOT_NULL(catch_info.variable);
    return factory()->NewTryCatchStatement(
        try_block, catch_info.scope, catch_info.variable, catch_block, pos);
  } else {
    DCHECK_NOT_NULL(finally_block);
    return factory()->NewTryFinallyStatement(try_block, finally_block, pos);
  }
}

// !%_IsJSReceiver(result = iterator.next()) &&
//     %ThrowIteratorResultNotAnObject(result)
Expression* Parser::BuildIteratorNextResult(Expression* iterator,
                                            Variable* result, int pos) {
  Expression* next_literal = factory()->NewStringLiteral(
      ast_value_factory()->next_string(), kNoSourcePosition);
  Expression* next_property =
      factory()->NewProperty(iterator, next_literal, kNoSourcePosition);
  ZoneList<Expression*>* next_arguments =
      new (zone()) ZoneList<Expression*>(0, zone());
  Expression* next_call =
      factory()->NewCall(next_property, next_arguments, pos);
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
                                              Statement* body,
                                              int each_keyword_pos) {
  ForOfStatement* for_of = stmt->AsForOfStatement();
  if (for_of != NULL) {
    const bool finalize = true;
    return InitializeForOfStatement(for_of, each, subject, body, finalize,
                                    each_keyword_pos);
  } else {
    if (each->IsArrayLiteral() || each->IsObjectLiteral()) {
      Variable* temp = NewTemporary(ast_value_factory()->empty_string());
      VariableProxy* temp_proxy = factory()->NewVariableProxy(temp);
      Expression* assign_each = PatternRewriter::RewriteDestructuringAssignment(
          this, factory()->NewAssignment(Token::ASSIGN, each, temp_proxy,
                                         kNoSourcePosition),
          scope());
      auto block = factory()->NewBlock(nullptr, 2, false, kNoSourcePosition);
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
    Block* init_block = factory()->NewBlock(
        nullptr, 2, true, for_info.parsing_result.descriptor.declaration_pos);
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
//     <let x' be a temporary variable>
//     for (x' in/of e) {
//       let/const/var x;
//       x = x';
//       b;
//     }
//     let x;  // for TDZ
//   }
void Parser::DesugarBindingInForEachStatement(ForInfo* for_info,
                                              Block** body_block,
                                              Expression** each_variable,
                                              bool* ok) {
  DCHECK(for_info->parsing_result.declarations.length() == 1);
  DeclarationParsingResult::Declaration& decl =
      for_info->parsing_result.declarations[0];
  Variable* temp = NewTemporary(ast_value_factory()->dot_for_string());
  auto each_initialization_block =
      factory()->NewBlock(nullptr, 1, true, kNoSourcePosition);
  {
    auto descriptor = for_info->parsing_result.descriptor;
    descriptor.declaration_pos = kNoSourcePosition;
    descriptor.initialization_pos = kNoSourcePosition;
    decl.initializer = factory()->NewVariableProxy(temp);

    bool is_for_var_of =
        for_info->mode == ForEachStatement::ITERATE &&
        for_info->parsing_result.descriptor.mode == VariableMode::VAR;

    PatternRewriter::DeclareAndInitializeVariables(
        this, each_initialization_block, &descriptor, &decl,
        (IsLexicalVariableMode(for_info->parsing_result.descriptor.mode) ||
         is_for_var_of)
            ? &for_info->bound_names
            : nullptr,
        CHECK_OK_VOID);

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
          auto name = catch_scope->catch_variable_name();
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

  *body_block = factory()->NewBlock(nullptr, 3, false, kNoSourcePosition);
  (*body_block)->statements()->Add(each_initialization_block, zone());
  *each_variable = factory()->NewVariableProxy(temp, for_info->position);
}

// Create a TDZ for any lexically-bound names in for in/of statements.
Block* Parser::CreateForEachStatementTDZ(Block* init_block,
                                         const ForInfo& for_info, bool* ok) {
  if (IsLexicalVariableMode(for_info.parsing_result.descriptor.mode)) {
    DCHECK_NULL(init_block);

    init_block = factory()->NewBlock(nullptr, 1, false, kNoSourcePosition);

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

Statement* Parser::InitializeForOfStatement(ForOfStatement* for_of,
                                            Expression* each,
                                            Expression* iterable,
                                            Statement* body, bool finalize,
                                            int next_result_pos) {
  // Create the auxiliary expressions needed for iterating over the iterable,
  // and initialize the given ForOfStatement with them.
  // If finalize is true, also instrument the loop with code that performs the
  // proper ES6 iterator finalization.  In that case, the result is not
  // immediately a ForOfStatement.

  const int nopos = kNoSourcePosition;
  auto avfactory = ast_value_factory();

  Variable* iterator = NewTemporary(avfactory->dot_iterator_string());
  Variable* result = NewTemporary(avfactory->dot_result_string());
  Variable* completion = NewTemporary(avfactory->empty_string());

  // iterator = GetIterator(iterable)
  Expression* assign_iterator;
  {
    assign_iterator = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(iterator),
        factory()->NewGetIterator(iterable, iterable->position()),
        iterable->position());
  }

  // !%_IsJSReceiver(result = iterator.next()) &&
  //     %ThrowIteratorResultNotAnObject(result)
  Expression* next_result;
  {
    Expression* iterator_proxy = factory()->NewVariableProxy(iterator);
    next_result =
        BuildIteratorNextResult(iterator_proxy, result, next_result_pos);
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

  // {{completion = kAbruptCompletion;}}
  Statement* set_completion_abrupt;
  if (finalize) {
    Expression* proxy = factory()->NewVariableProxy(completion);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, proxy,
        factory()->NewSmiLiteral(Parser::kAbruptCompletion, nopos), nopos);

    Block* block = factory()->NewBlock(nullptr, 1, true, nopos);
    block->statements()->Add(
        factory()->NewExpressionStatement(assignment, nopos), zone());
    set_completion_abrupt = block;
  }

  // do { let tmp = #result_value; #set_completion_abrupt; tmp }
  // Expression* result_value (gets overwritten)
  if (finalize) {
    Variable* var_tmp = NewTemporary(avfactory->empty_string());
    Expression* tmp = factory()->NewVariableProxy(var_tmp);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, tmp, result_value, nopos);

    Block* block = factory()->NewBlock(nullptr, 2, false, nopos);
    block->statements()->Add(
        factory()->NewExpressionStatement(assignment, nopos), zone());
    block->statements()->Add(set_completion_abrupt, zone());

    result_value = factory()->NewDoExpression(block, var_tmp, nopos);
  }

  // each = #result_value;
  Expression* assign_each;
  {
    assign_each =
        factory()->NewAssignment(Token::ASSIGN, each, result_value, nopos);
    if (each->IsArrayLiteral() || each->IsObjectLiteral()) {
      assign_each = PatternRewriter::RewriteDestructuringAssignment(
          this, assign_each->AsAssignment(), scope());
    }
  }

  // {{completion = kNormalCompletion;}}
  Statement* set_completion_normal;
  if (finalize) {
    Expression* proxy = factory()->NewVariableProxy(completion);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, proxy,
        factory()->NewSmiLiteral(Parser::kNormalCompletion, nopos), nopos);

    Block* block = factory()->NewBlock(nullptr, 1, true, nopos);
    block->statements()->Add(
        factory()->NewExpressionStatement(assignment, nopos), zone());
    set_completion_normal = block;
  }

  // { #loop-body; #set_completion_normal }
  // Statement* body (gets overwritten)
  if (finalize) {
    Block* block = factory()->NewBlock(nullptr, 2, false, nopos);
    block->statements()->Add(body, zone());
    block->statements()->Add(set_completion_normal, zone());
    body = block;
  }

  for_of->Initialize(body, iterator, assign_iterator, next_result, result_done,
                     assign_each);
  return finalize ? FinalizeForOfStatement(for_of, completion, nopos) : for_of;
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

  DCHECK(for_info.bound_names.length() > 0);
  ZoneList<Variable*> temps(for_info.bound_names.length(), zone());

  Block* outer_block = factory()->NewBlock(
      nullptr, for_info.bound_names.length() + 4, false, kNoSourcePosition);

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

  Variable* first = NULL;
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
      factory()->NewForStatement(NULL, kNoSourcePosition);
  outer_block->statements()->Add(outer_loop, zone());
  outer_block->set_scope(scope());

  Block* inner_block = factory()->NewBlock(NULL, 3, false, kNoSourcePosition);
  {
    BlockState block_state(&scope_state_, inner_scope);

    Block* ignore_completion_block = factory()->NewBlock(
        nullptr, for_info.bound_names.length() + 3, true, kNoSourcePosition);
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
      DCHECK(init->position() != kNoSourcePosition);
      decl->proxy()->var()->set_initializer_position(init->position());
      ignore_completion_block->statements()->Add(assignment_statement, zone());
    }

    // Make statement: if (first == 1) { first = 0; } else { next; }
    if (next) {
      DCHECK(first);
      Expression* compare = NULL;
      // Make compare expression: first == 1.
      {
        Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
        VariableProxy* first_proxy = factory()->NewVariableProxy(first);
        compare = factory()->NewCompareOperation(Token::EQ, first_proxy, const1,
                                                 kNoSourcePosition);
      }
      Statement* clear_first = NULL;
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
    Expression* flag_cond = NULL;
    {
      Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      flag_cond = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                                 kNoSourcePosition);
    }

    // Create chain of expressions "flag = 0, temp_x = x, ..."
    Statement* compound_next_statement = NULL;
    {
      Expression* compound_next = NULL;
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
    loop->Initialize(NULL, flag_cond, compound_next_statement, body);
    inner_block->statements()->Add(loop, zone());

    // Make statement: {{if (flag == 1) break;}}
    {
      Expression* compare = NULL;
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
      Block* ignore_completion_block =
          factory()->NewBlock(NULL, 1, true, kNoSourcePosition);
      ignore_completion_block->statements()->Add(if_flag_break, zone());
      inner_block->statements()->Add(ignore_completion_block, zone());
    }

    inner_scope->set_end_position(scanner()->location().end_pos);
    inner_block->set_scope(inner_scope);
  }

  outer_loop->Initialize(NULL, NULL, NULL, inner_block);
  return outer_block;
}

void Parser::AddArrowFunctionFormalParameters(
    ParserFormalParameters* parameters, Expression* expr, int end_pos,
    bool* ok) {
  // ArrowFunctionFormals ::
  //    Binary(Token::COMMA, NonTailArrowFunctionFormals, Tail)
  //    Tail
  // NonTailArrowFunctionFormals ::
  //    Binary(Token::COMMA, NonTailArrowFunctionFormals, VariableProxy)
  //    VariableProxy
  // Tail ::
  //    VariableProxy
  //    Spread(VariableProxy)
  //
  // As we need to visit the parameters in left-to-right order, we recurse on
  // the left-hand side of comma expressions.
  //
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
  if (parameters->is_simple) {
    parameters->is_simple = !is_rest && expr->IsVariableProxy();
  }

  Expression* initializer = nullptr;
  if (expr->IsAssignment()) {
    Assignment* assignment = expr->AsAssignment();
    DCHECK(!assignment->is_compound());
    initializer = assignment->value();
    expr = assignment->target();
  }

  AddFormalParameter(parameters, expr, initializer, end_pos, is_rest);
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

  ExpressionClassifier classifier(this);
  if (!parameters->is_simple) {
    this->classifier()->RecordNonSimpleParameter();
  }
  DeclareFormalParameters(parameters->scope, parameters->params);
  if (!this->classifier()
           ->is_valid_formal_parameter_list_without_duplicates()) {
    *duplicate_loc =
        this->classifier()->duplicate_formal_parameter_error().location;
  }
  DCHECK_EQ(parameters->is_simple, parameters->scope->has_simple_parameters());
}

void Parser::ReindexLiterals(const ParserFormalParameters& parameters) {
  if (function_state_->materialized_literal_count() > 0) {
    AstLiteralReindexer reindexer;

    for (auto p : parameters.params) {
      if (p->pattern != nullptr) reindexer.Reindex(p->pattern);
      if (p->initializer != nullptr) reindexer.Reindex(p->initializer);
    }

    DCHECK(reindexer.count() <= function_state_->materialized_literal_count());
  }
}

void Parser::PrepareGeneratorVariables() {
  // For generators, allocating variables in contexts is currently a win because
  // it minimizes the work needed to suspend and resume an activation.  The
  // code produced for generators relies on this forced context allocation (it
  // does not restore the frame's parameters upon resume).
  function_state_->scope()->ForceContextAllocation();

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
    LanguageMode language_mode, bool* ok) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'
  //
  // Getter ::
  //   '(' ')' '{' FunctionBody '}'
  //
  // Setter ::
  //   '(' PropertySetParameterList ')' '{' FunctionBody '}'

  int pos = function_token_pos == kNoSourcePosition ? peek_position()
                                                    : function_token_pos;

  // Anonymous functions were passed either the empty symbol or a null
  // handle as the function name.  Remember if we were passed a non-empty
  // handle to decide whether to invoke function name inference.
  bool should_infer_name = function_name == NULL;

  // We want a non-null handle as the function name.
  if (should_infer_name) {
    function_name = ast_value_factory()->empty_string();
  }

  FunctionLiteral::EagerCompileHint eager_compile_hint =
      function_state_->next_function_is_likely_called()
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

  bool can_preparse = parse_lazily() &&
                      eager_compile_hint == FunctionLiteral::kShouldLazyCompile;

  bool is_lazy_top_level_function =
      can_preparse && impl()->AllowsLazyParsingWithoutUnresolvedVariables();

  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_,
      parsing_on_main_thread_
          ? &RuntimeCallStats::ParseFunctionLiteral
          : &RuntimeCallStats::ParseBackgroundFunctionLiteral);

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
  // TODO(marja): Refactor parsing modes: simplify this.
  bool use_temp_zone =
      (FLAG_aggressive_lazy_inner_functions
           ? can_preparse
           : (is_lazy_top_level_function ||
              (parse_lazily() &&
               function_type == FunctionLiteral::kDeclaration &&
               eager_compile_hint == FunctionLiteral::kShouldLazyCompile)));

  DCHECK_IMPLIES(
      (is_lazy_top_level_function ||
       (parse_lazily() && function_type == FunctionLiteral::kDeclaration &&
        eager_compile_hint == FunctionLiteral::kShouldLazyCompile)),
      can_preparse);
  bool is_lazy_inner_function =
      use_temp_zone && FLAG_lazy_inner_functions && !is_lazy_top_level_function;

  ZoneList<Statement*>* body = nullptr;
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  bool should_be_used_once_hint = false;
  int num_parameters = -1;
  int function_length = -1;
  bool has_duplicate_parameters = false;
  int function_literal_id = GetNextFunctionLiteralId();

  Zone* outer_zone = zone();
  DeclarationScope* scope;

  {
    // Temporary zones can nest. When we migrate free variables (see below), we
    // need to recreate them in the previous Zone.
    AstNodeFactory previous_zone_ast_node_factory(ast_value_factory());
    previous_zone_ast_node_factory.set_zone(zone());

    // Open a new zone scope, which sets our AstNodeFactory to allocate in the
    // new temporary zone if the preconditions are satisfied, and ensures that
    // the previous zone is always restored after parsing the body. To be able
    // to do scope analysis correctly after full parsing, we migrate needed
    // information when the function is parsed.
    Zone temp_zone(zone()->allocator(), ZONE_NAME);
    DiscardableZoneScope zone_scope(this, &temp_zone, use_temp_zone);

    // This Scope lives in the main zone. We'll migrate data into that zone
    // later.
    scope = NewFunctionScope(kind, outer_zone);
    SetLanguageMode(scope, language_mode);
#ifdef DEBUG
    scope->SetScopeName(function_name);
    if (use_temp_zone) scope->set_needs_migration();
#endif

    Expect(Token::LPAREN, CHECK_OK);
    scope->set_start_position(scanner()->location().beg_pos);

    // Eager or lazy parse? If is_lazy_top_level_function, we'll parse
    // lazily. We'll call SkipFunction, which may decide to
    // abort lazy parsing if it suspects that wasn't a good idea. If so (in
    // which case the parser is expected to have backtracked), or if we didn't
    // try to lazy parse in the first place, we'll have to parse eagerly.
    if (is_lazy_top_level_function || is_lazy_inner_function) {
      Scanner::BookmarkScope bookmark(scanner());
      bookmark.Set();
      LazyParsingResult result =
          SkipFunction(kind, scope, &num_parameters, &function_length,
                       &has_duplicate_parameters, &materialized_literal_count,
                       &expected_property_count, is_lazy_inner_function,
                       is_lazy_top_level_function, CHECK_OK);

      if (result == kLazyParsingAborted) {
        DCHECK(is_lazy_top_level_function);
        bookmark.Apply();
        // Trigger eager (re-)parsing, just below this block.
        is_lazy_top_level_function = false;

        // This is probably an initialization function. Inform the compiler it
        // should also eager-compile this function, and that we expect it to be
        // used once.
        eager_compile_hint = FunctionLiteral::kShouldEagerCompile;
        should_be_used_once_hint = true;
        scope->ResetAfterPreparsing(ast_value_factory(), true);
        zone_scope.Reset();
        use_temp_zone = false;
      }
    }

    if (!is_lazy_top_level_function && !is_lazy_inner_function) {
      body = ParseFunction(
          function_name, pos, kind, function_type, scope, &num_parameters,
          &function_length, &has_duplicate_parameters,
          &materialized_literal_count, &expected_property_count, CHECK_OK);
    }

    DCHECK(use_temp_zone || !is_lazy_top_level_function);
    if (use_temp_zone) {
      // If the preconditions are correct the function body should never be
      // accessed, but do this anyway for better behaviour if they're wrong.
      body = nullptr;
      scope->AnalyzePartially(&previous_zone_ast_node_factory);
    }

    DCHECK_IMPLIES(use_temp_zone, temp_zoned_);
    if (FLAG_trace_preparse) {
      PrintF("  [%s]: %i-%i %.*s\n",
             is_lazy_top_level_function
                 ? "Preparse no-resolution"
                 : (temp_zoned_ ? "Preparse resolution" : "Full parse"),
             scope->start_position(), scope->end_position(),
             function_name->byte_length(), function_name->raw_data());
    }
    if (V8_UNLIKELY(FLAG_runtime_stats)) {
      if (is_lazy_top_level_function) {
        RuntimeCallStats::CorrectCurrentCounterId(
            runtime_call_stats_,
            parsing_on_main_thread_
                ? &RuntimeCallStats::PreParseNoVariableResolution
                : &RuntimeCallStats::PreParseBackgroundNoVariableResolution);
      } else if (temp_zoned_) {
        RuntimeCallStats::CorrectCurrentCounterId(
            runtime_call_stats_,
            parsing_on_main_thread_
                ? &RuntimeCallStats::PreParseWithVariableResolution
                : &RuntimeCallStats::PreParseBackgroundWithVariableResolution);
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
      function_name, scope, body, materialized_literal_count,
      expected_property_count, num_parameters, function_length,
      duplicate_parameters, function_type, eager_compile_hint, pos, true,
      function_literal_id);
  function_literal->set_function_token_position(function_token_pos);
  if (should_be_used_once_hint)
    function_literal->set_should_be_used_once_hint();

  if (should_infer_name) {
    DCHECK_NOT_NULL(fni_);
    fni_->AddFunction(function_literal);
  }
  return function_literal;
}

Parser::LazyParsingResult Parser::SkipFunction(
    FunctionKind kind, DeclarationScope* function_scope, int* num_parameters,
    int* function_length, bool* has_duplicate_parameters,
    int* materialized_literal_count, int* expected_property_count,
    bool is_inner_function, bool may_abort, bool* ok) {
  DCHECK_NE(kNoSourcePosition, function_scope->start_position());
  if (produce_cached_parse_data()) CHECK(log_);

  DCHECK_IMPLIES(IsArrowFunction(kind),
                 scanner()->current_token() == Token::ARROW);

  // Inner functions are not part of the cached data.
  if (!is_inner_function && consume_cached_parse_data() &&
      !cached_parse_data_->rejected()) {
    // If we have cached data, we use it to skip parsing the function. The data
    // contains the information we need to construct the lazy function.
    FunctionEntry entry =
        cached_parse_data_->GetFunctionEntry(function_scope->start_position());
    // Check that cached data is valid. If not, mark it as invalid (the embedder
    // handles it). Note that end position greater than end of stream is safe,
    // and hard to check.
    if (entry.is_valid() &&
        entry.end_pos() > function_scope->start_position()) {
      total_preparse_skipped_ += entry.end_pos() - position();
      function_scope->set_end_position(entry.end_pos());
      scanner()->SeekForward(entry.end_pos() - 1);
      Expect(Token::RBRACE, CHECK_OK_VALUE(kLazyParsingComplete));
      *num_parameters = entry.num_parameters();
      *function_length = entry.function_length();
      *has_duplicate_parameters = entry.has_duplicate_parameters();
      *materialized_literal_count = entry.literal_count();
      *expected_property_count = entry.property_count();
      SetLanguageMode(function_scope, entry.language_mode());
      if (entry.uses_super_property())
        function_scope->RecordSuperPropertyUsage();
      if (entry.calls_eval()) function_scope->RecordEvalCall();
      SkipFunctionLiterals(entry.num_inner_functions());
      return kLazyParsingComplete;
    }
    cached_parse_data_->Reject();
  }

  // With no cached data, we partially parse the function, without building an
  // AST. This gathers the data needed to build a lazy function.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.PreParse");

  if (reusable_preparser_ == NULL) {
    reusable_preparser_ = new PreParser(
        zone(), &scanner_, stack_limit_, ast_value_factory(),
        &pending_error_handler_, runtime_call_stats_, parsing_on_main_thread_);
#define SET_ALLOW(name) reusable_preparser_->set_allow_##name(allow_##name());
    SET_ALLOW(natives);
    SET_ALLOW(harmony_do_expressions);
    SET_ALLOW(harmony_function_sent);
    SET_ALLOW(harmony_trailing_commas);
    SET_ALLOW(harmony_class_fields);
    SET_ALLOW(harmony_object_spread);
#undef SET_ALLOW
  }
  // Aborting inner function preparsing would leave scopes in an inconsistent
  // state; we don't parse inner functions in the abortable mode anyway.
  DCHECK(!is_inner_function || !may_abort);

  PreParser::PreParseResult result = reusable_preparser_->PreParseFunction(
      kind, function_scope, parsing_module_, is_inner_function, may_abort,
      use_counts_);

  // Return immediately if pre-parser decided to abort parsing.
  if (result == PreParser::kPreParseAbort) return kLazyParsingAborted;
  if (result == PreParser::kPreParseStackOverflow) {
    // Propagate stack overflow.
    set_stack_overflow();
    *ok = false;
    return kLazyParsingComplete;
  }
  if (pending_error_handler_.has_pending_error()) {
    *ok = false;
    return kLazyParsingComplete;
  }
  PreParserLogger* logger = reusable_preparser_->logger();
  function_scope->set_end_position(logger->end());
  Expect(Token::RBRACE, CHECK_OK_VALUE(kLazyParsingComplete));
  total_preparse_skipped_ +=
      function_scope->end_position() - function_scope->start_position();
  *num_parameters = logger->num_parameters();
  *function_length = logger->function_length();
  *has_duplicate_parameters = logger->has_duplicate_parameters();
  *materialized_literal_count = logger->literals();
  *expected_property_count = logger->properties();
  SkipFunctionLiterals(logger->num_inner_functions());
  if (!is_inner_function && produce_cached_parse_data()) {
    DCHECK(log_);
    log_->LogFunction(
        function_scope->start_position(), function_scope->end_position(),
        *num_parameters, *function_length, *has_duplicate_parameters,
        *materialized_literal_count, *expected_property_count, language_mode(),
        function_scope->uses_super_property(), function_scope->calls_eval(),
        logger->num_inner_functions());
  }
  return kLazyParsingComplete;
}


Statement* Parser::BuildAssertIsCoercible(Variable* var) {
  // if (var === null || var === undefined)
  //     throw /* type error kNonCoercible) */;

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
      NewThrowTypeError(MessageTemplate::kNonCoercible,
                        ast_value_factory()->empty_string(), kNoSourcePosition);
  IfStatement* if_statement = factory()->NewIfStatement(
      condition,
      factory()->NewExpressionStatement(throw_type_error, kNoSourcePosition),
      factory()->NewEmptyStatement(kNoSourcePosition), kNoSourcePosition);
  return if_statement;
}


class InitializerRewriter final
    : public AstTraversalVisitor<InitializerRewriter> {
 public:
  InitializerRewriter(uintptr_t stack_limit, Expression* root, Parser* parser,
                      Scope* scope)
      : AstTraversalVisitor(stack_limit, root),
        parser_(parser),
        scope_(scope) {}

 private:
  // This is required so that the overriden Visit* methods can be
  // called by the base class (template).
  friend class AstTraversalVisitor<InitializerRewriter>;

  // Just rewrite destructuring assignments wrapped in RewritableExpressions.
  void VisitRewritableExpression(RewritableExpression* to_rewrite) {
    if (to_rewrite->is_rewritten()) return;
    Parser::PatternRewriter::RewriteDestructuringAssignment(parser_, to_rewrite,
                                                            scope_);
    AstTraversalVisitor::VisitRewritableExpression(to_rewrite);
  }

  // Code in function literals does not need to be eagerly rewritten, it will be
  // rewritten when scheduled.
  void VisitFunctionLiteral(FunctionLiteral* expr) {}

  Parser* parser_;
  Scope* scope_;
};


void Parser::RewriteParameterInitializer(Expression* expr, Scope* scope) {
  InitializerRewriter rewriter(stack_limit_, expr, this, scope);
  rewriter.Run();
}


Block* Parser::BuildParameterInitializationBlock(
    const ParserFormalParameters& parameters, bool* ok) {
  DCHECK(!parameters.is_simple);
  DCHECK(scope()->is_function_scope());
  Block* init_block = factory()->NewBlock(NULL, 1, true, kNoSourcePosition);
  int index = 0;
  for (auto parameter : parameters.params) {
    if (parameter->is_rest && parameter->pattern->IsVariableProxy()) break;
    DeclarationDescriptor descriptor;
    descriptor.declaration_kind = DeclarationDescriptor::PARAMETER;
    descriptor.scope = scope();
    descriptor.hoist_scope = nullptr;
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
      RewriteParameterInitializer(parameter->initializer, scope());

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
    if (!parameter->is_simple() && scope()->calls_sloppy_eval()) {
      param_scope = NewVarblockScope();
      param_scope->set_start_position(descriptor.initialization_pos);
      param_scope->set_end_position(parameter->initializer_end_position);
      param_scope->RecordEvalCall();
      param_block = factory()->NewBlock(NULL, 8, true, kNoSourcePosition);
      param_block->set_scope(param_scope);
      descriptor.hoist_scope = scope();
      // Pass the appropriate scope in so that PatternRewriter can appropriately
      // rewrite inner initializers of the pattern to param_scope
      descriptor.scope = param_scope;
      // Rewrite the outer initializer to point to param_scope
      ReparentParameterExpressionScope(stack_limit(), initial_value,
                                       param_scope);
    }

    BlockState block_state(&scope_state_, param_scope);
    DeclarationParsingResult::Declaration decl(
        parameter->pattern, parameter->initializer_end_position, initial_value);
    PatternRewriter::DeclareAndInitializeVariables(
        this, param_block, &descriptor, &decl, nullptr, CHECK_OK);

    if (param_block != init_block) {
      param_scope = block_state.FinalizedBlockScope();
      if (param_scope != nullptr) {
        CheckConflictingVarDeclarations(param_scope, CHECK_OK);
      }
      init_block->statements()->Add(param_block, zone());
    }
    ++index;
  }
  return init_block;
}

Block* Parser::BuildRejectPromiseOnException(Block* inner_block, bool* ok) {
  // .promise = %AsyncFunctionPromiseCreate();
  // try {
  //   <inner_block>
  // } catch (.catch) {
  //   %RejectPromise(.promise, .catch);
  //   return .promise;
  // } finally {
  //   %AsyncFunctionPromiseRelease(.promise);
  // }
  Block* result = factory()->NewBlock(nullptr, 2, true, kNoSourcePosition);

  // .promise = %AsyncFunctionPromiseCreate();
  Statement* set_promise;
  {
    Expression* create_promise = factory()->NewCallRuntime(
        Context::ASYNC_FUNCTION_PROMISE_CREATE_INDEX,
        new (zone()) ZoneList<Expression*>(0, zone()), kNoSourcePosition);
    Assignment* assign_promise = factory()->NewAssignment(
        Token::INIT, factory()->NewVariableProxy(PromiseVariable()),
        create_promise, kNoSourcePosition);
    set_promise =
        factory()->NewExpressionStatement(assign_promise, kNoSourcePosition);
  }
  result->statements()->Add(set_promise, zone());

  // catch (.catch) { return %RejectPromise(.promise, .catch), .promise }
  Scope* catch_scope = NewScope(CATCH_SCOPE);
  catch_scope->set_is_hidden();
  Variable* catch_variable =
      catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(), VAR);
  Block* catch_block = factory()->NewBlock(nullptr, 1, true, kNoSourcePosition);

  Expression* promise_reject = BuildRejectPromise(
      factory()->NewVariableProxy(catch_variable), kNoSourcePosition);
  ReturnStatement* return_promise_reject =
      factory()->NewReturnStatement(promise_reject, kNoSourcePosition);
  catch_block->statements()->Add(return_promise_reject, zone());

  TryStatement* try_catch_statement =
      factory()->NewTryCatchStatementForAsyncAwait(inner_block, catch_scope,
                                                   catch_variable, catch_block,
                                                   kNoSourcePosition);

  // There is no TryCatchFinally node, so wrap it in an outer try/finally
  Block* outer_try_block =
      factory()->NewBlock(nullptr, 1, true, kNoSourcePosition);
  outer_try_block->statements()->Add(try_catch_statement, zone());

  // finally { %AsyncFunctionPromiseRelease(.promise) }
  Block* finally_block =
      factory()->NewBlock(nullptr, 1, true, kNoSourcePosition);
  {
    ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(1, zone());
    args->Add(factory()->NewVariableProxy(PromiseVariable()), zone());
    Expression* call_promise_release = factory()->NewCallRuntime(
        Context::ASYNC_FUNCTION_PROMISE_RELEASE_INDEX, args, kNoSourcePosition);
    Statement* promise_release = factory()->NewExpressionStatement(
        call_promise_release, kNoSourcePosition);
    finally_block->statements()->Add(promise_release, zone());
  }

  Statement* try_finally_statement = factory()->NewTryFinallyStatement(
      outer_try_block, finally_block, kNoSourcePosition);

  result->statements()->Add(try_finally_statement, zone());
  return result;
}

Assignment* Parser::BuildCreateJSGeneratorObject(int pos, FunctionKind kind) {
  // .generator = %CreateJSGeneratorObject(...);
  DCHECK_NOT_NULL(function_state_->generator_object_variable());
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(factory()->NewThisFunction(pos), zone());
  args->Add(IsArrowFunction(kind) ? GetLiteralUndefined(pos)
                                  : ThisExpression(kNoSourcePosition),
            zone());
  Expression* allocation =
      factory()->NewCallRuntime(Runtime::kCreateJSGeneratorObject, args, pos);
  VariableProxy* proxy =
      factory()->NewVariableProxy(function_state_->generator_object_variable());
  return factory()->NewAssignment(Token::INIT, proxy, allocation,
                                  kNoSourcePosition);
}

Expression* Parser::BuildResolvePromise(Expression* value, int pos) {
  // %ResolvePromise(.promise, value), .promise
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(factory()->NewVariableProxy(PromiseVariable()), zone());
  args->Add(value, zone());
  Expression* call_runtime =
      factory()->NewCallRuntime(Context::PROMISE_RESOLVE_INDEX, args, pos);
  return factory()->NewBinaryOperation(
      Token::COMMA, call_runtime,
      factory()->NewVariableProxy(PromiseVariable()), pos);
}

Expression* Parser::BuildRejectPromise(Expression* value, int pos) {
  // %RejectPromiseNoDebugEvent(.promise, value, true), .promise
  // The NoDebugEvent variant disables the additional debug event for the
  // rejection since a debug event already happened for the exception that got
  // us here.
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(factory()->NewVariableProxy(PromiseVariable()), zone());
  args->Add(value, zone());
  Expression* call_runtime = factory()->NewCallRuntime(
      Context::REJECT_PROMISE_NO_DEBUG_EVENT_INDEX, args, pos);
  return factory()->NewBinaryOperation(
      Token::COMMA, call_runtime,
      factory()->NewVariableProxy(PromiseVariable()), pos);
}

Variable* Parser::PromiseVariable() {
  // Based on the various compilation paths, there are many different code
  // paths which may be the first to access the Promise temporary. Whichever
  // comes first should create it and stash it in the FunctionState.
  Variable* promise = function_state_->promise_variable();
  if (function_state_->promise_variable() == nullptr) {
    promise = function_state_->scope()->DeclarePromiseVar(
        ast_value_factory()->empty_string());
  }
  return promise;
}

Expression* Parser::BuildInitialYield(int pos, FunctionKind kind) {
  Assignment* assignment = BuildCreateJSGeneratorObject(pos, kind);
  VariableProxy* generator =
      factory()->NewVariableProxy(function_state_->generator_object_variable());
  // The position of the yield is important for reporting the exception
  // caused by calling the .throw method on a generator suspended at the
  // initial yield (i.e. right after generator instantiation).
  return factory()->NewYield(generator, assignment, scope()->start_position(),
                             Yield::kOnExceptionThrow);
}

ZoneList<Statement*>* Parser::ParseFunction(
    const AstRawString* function_name, int pos, FunctionKind kind,
    FunctionLiteral::FunctionType function_type,
    DeclarationScope* function_scope, int* num_parameters, int* function_length,
    bool* has_duplicate_parameters, int* materialized_literal_count,
    int* expected_property_count, bool* ok) {
  ParsingModeScope mode(this, allow_lazy_ ? PARSE_LAZILY : PARSE_EAGERLY);

  FunctionState function_state(&function_state_, &scope_state_, function_scope);

  DuplicateFinder duplicate_finder;
  ExpressionClassifier formals_classifier(this, &duplicate_finder);

  if (IsResumableFunction(kind)) PrepareGeneratorVariables();

  ParserFormalParameters formals(function_scope);
  ParseFormalParameterList(&formals, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  int formals_end_position = scanner()->location().end_pos;
  *num_parameters = formals.num_parameters();
  *function_length = formals.function_length;

  CheckArityRestrictions(formals.arity, kind, formals.has_rest,
                         function_scope->start_position(), formals_end_position,
                         CHECK_OK);
  Expect(Token::LBRACE, CHECK_OK);

  ZoneList<Statement*>* body = ParseEagerFunctionBody(
      function_name, pos, formals, kind, function_type, ok);

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

  *materialized_literal_count = function_state.materialized_literal_count();
  *expected_property_count = function_state.expected_property_count();
  return body;
}

ZoneList<Statement*>* Parser::ParseEagerFunctionBody(
    const AstRawString* function_name, int pos,
    const ParserFormalParameters& parameters, FunctionKind kind,
    FunctionLiteral::FunctionType function_type, bool* ok) {
  ZoneList<Statement*>* result = new(zone()) ZoneList<Statement*>(8, zone());

  static const int kFunctionNameAssignmentIndex = 0;
  if (function_type == FunctionLiteral::kNamedExpression) {
    DCHECK(function_name != NULL);
    // If we have a named function expression, we add a local variable
    // declaration to the body of the function with the name of the
    // function and let it refer to the function itself (closure).
    // Not having parsed the function body, the language mode may still change,
    // so we reserve a spot and create the actual const assignment later.
    DCHECK_EQ(kFunctionNameAssignmentIndex, result->length());
    result->Add(NULL, zone());
  }

  ZoneList<Statement*>* body = result;
  DeclarationScope* function_scope = scope()->AsDeclarationScope();
  DeclarationScope* inner_scope = function_scope;
  Block* inner_block = nullptr;
  if (!parameters.is_simple) {
    inner_scope = NewVarblockScope();
    inner_scope->set_start_position(scanner()->location().beg_pos);
    inner_block = factory()->NewBlock(NULL, 8, true, kNoSourcePosition);
    inner_block->set_scope(inner_scope);
    body = inner_block->statements();
  }

  {
    BlockState block_state(&scope_state_, inner_scope);

    if (IsGeneratorFunction(kind)) {
      // We produce:
      //
      // try { InitialYield; ...body...; return {value: undefined, done: true} }
      // finally { %_GeneratorClose(generator) }
      //
      // - InitialYield yields the actual generator object.
      // - Any return statement inside the body will have its argument wrapped
      //   in a "done" iterator result object.
      // - If the generator terminates for whatever reason, we must close it.
      //   Hence the finally clause.

      Block* try_block =
          factory()->NewBlock(nullptr, 3, false, kNoSourcePosition);
      Expression* initial_yield = BuildInitialYield(pos, kind);
      try_block->statements()->Add(
          factory()->NewExpressionStatement(initial_yield, kNoSourcePosition),
          zone());
      ParseStatementList(try_block->statements(), Token::RBRACE, CHECK_OK);

      Statement* final_return = factory()->NewReturnStatement(
          BuildIteratorResult(nullptr, true), kNoSourcePosition);
      try_block->statements()->Add(final_return, zone());

      Block* finally_block =
          factory()->NewBlock(nullptr, 1, false, kNoSourcePosition);
      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(1, zone());
      VariableProxy* call_proxy = factory()->NewVariableProxy(
          function_state_->generator_object_variable());
      args->Add(call_proxy, zone());
      Expression* call = factory()->NewCallRuntime(
          Runtime::kInlineGeneratorClose, args, kNoSourcePosition);
      finally_block->statements()->Add(
          factory()->NewExpressionStatement(call, kNoSourcePosition), zone());

      body->Add(factory()->NewTryFinallyStatement(try_block, finally_block,
                                                  kNoSourcePosition),
                zone());
    } else if (IsAsyncFunction(kind)) {
      const bool accept_IN = true;
      ParseAsyncFunctionBody(inner_scope, body, kind, FunctionBodyType::kNormal,
                             accept_IN, pos, CHECK_OK);
    } else {
      ParseStatementList(body, Token::RBRACE, CHECK_OK);
    }

    if (IsDerivedConstructor(kind)) {
      body->Add(factory()->NewReturnStatement(ThisExpression(kNoSourcePosition),
                                              kNoSourcePosition),
                zone());
    }
  }

  Expect(Token::RBRACE, CHECK_OK);
  scope()->set_end_position(scanner()->location().end_pos);

  if (!parameters.is_simple) {
    DCHECK_NOT_NULL(inner_scope);
    DCHECK_EQ(function_scope, scope());
    DCHECK_EQ(function_scope, inner_scope->outer_scope());
    DCHECK_EQ(body, inner_block->statements());
    SetLanguageMode(function_scope, inner_scope->language_mode());
    Block* init_block = BuildParameterInitializationBlock(parameters, CHECK_OK);

    if (is_sloppy(inner_scope->language_mode())) {
      InsertSloppyBlockFunctionVarBindings(inner_scope);
    }

    // TODO(littledan): Merge the two rejection blocks into one
    if (IsAsyncFunction(kind)) {
      init_block = BuildRejectPromiseOnException(init_block, CHECK_OK);
    }

    DCHECK_NOT_NULL(init_block);

    inner_scope->set_end_position(scanner()->location().end_pos);
    if (inner_scope->FinalizeBlockScope() != nullptr) {
      CheckConflictingVarDeclarations(inner_scope, CHECK_OK);
      InsertShadowingVarBindingInitializers(inner_block);
    }
    inner_scope = nullptr;

    result->Add(init_block, zone());
    result->Add(inner_block, zone());
  } else {
    DCHECK_EQ(inner_scope, function_scope);
    if (is_sloppy(function_scope->language_mode())) {
      InsertSloppyBlockFunctionVarBindings(function_scope);
    }
  }

  if (!IsArrowFunction(kind)) {
    // Declare arguments after parsing the function since lexical 'arguments'
    // masks the arguments object. Declare arguments before declaring the
    // function var since the arguments object masks 'function arguments'.
    function_scope->DeclareArguments(ast_value_factory());
  }

  if (function_type == FunctionLiteral::kNamedExpression) {
    Statement* statement;
    if (function_scope->LookupLocal(function_name) == nullptr) {
      // Now that we know the language mode, we can create the const assignment
      // in the previously reserved spot.
      DCHECK_EQ(function_scope, scope());
      Variable* fvar = function_scope->DeclareFunctionVar(function_name);
      VariableProxy* fproxy = factory()->NewVariableProxy(fvar);
      statement = factory()->NewExpressionStatement(
          factory()->NewAssignment(Token::INIT, fproxy,
                                   factory()->NewThisFunction(pos),
                                   kNoSourcePosition),
          kNoSourcePosition);
    } else {
      statement = factory()->NewEmptyStatement(kNoSourcePosition);
    }
    result->Set(kFunctionNameAssignmentIndex, statement);
  }

  MarkCollectedTailCallExpressions();
  return result;
}

void Parser::DeclareClassVariable(const AstRawString* name, Scope* block_scope,
                                  ClassInfo* class_info, int class_token_pos,
                                  bool* ok) {
#ifdef DEBUG
  scope()->SetScopeName(name);
#endif

  if (name != nullptr) {
    class_info->proxy = factory()->NewVariableProxy(name, NORMAL_VARIABLE);
    Declaration* declaration = factory()->NewVariableDeclaration(
        class_info->proxy, block_scope, class_token_pos);
    Declare(declaration, DeclarationDescriptor::NORMAL, CONST,
            Variable::DefaultInitializationFlag(CONST), ok);
  }
}

// This method declares a property of the given class.  It updates the
// following fields of class_info, as appropriate:
//   - constructor
//   - properties
void Parser::DeclareClassProperty(const AstRawString* class_name,
                                  ClassLiteralProperty* property,
                                  ClassLiteralProperty::Kind kind,
                                  bool is_static, bool is_constructor,
                                  ClassInfo* class_info, bool* ok) {
  if (is_constructor) {
    DCHECK(!class_info->constructor);
    class_info->constructor = GetPropertyValue(property)->AsFunctionLiteral();
    DCHECK_NOT_NULL(class_info->constructor);
    class_info->constructor->set_raw_name(
        class_name != nullptr ? class_name
                              : ast_value_factory()->empty_string());
    return;
  }

  if (property->kind() == ClassLiteralProperty::FIELD) {
    DCHECK(allow_harmony_class_fields());
    // TODO(littledan): Implement class fields
  }
  class_info->properties->Add(property, zone());
}

// This method rewrites a class literal into a do-expression.
// It uses the following fields of class_info:
//   - constructor (if missing, it updates it with a default constructor)
//   - proxy
//   - extends
//   - properties
//   - has_name_static_property
//   - has_static_computed_names
Expression* Parser::RewriteClassLiteral(const AstRawString* name,
                                        ClassInfo* class_info, int pos,
                                        bool* ok) {
  int end_pos = scanner()->location().end_pos;
  Block* do_block = factory()->NewBlock(nullptr, 1, false, pos);
  Variable* result_var = NewTemporary(ast_value_factory()->empty_string());
  DoExpression* do_expr = factory()->NewDoExpression(do_block, result_var, pos);

  bool has_extends = class_info->extends != nullptr;
  bool has_default_constructor = class_info->constructor == nullptr;
  if (has_default_constructor) {
    class_info->constructor =
        DefaultConstructor(name, has_extends, pos, end_pos);
  }

  scope()->set_end_position(end_pos);

  if (name != nullptr) {
    DCHECK_NOT_NULL(class_info->proxy);
    class_info->proxy->var()->set_initializer_position(end_pos);
  }

  ClassLiteral* class_literal = factory()->NewClassLiteral(
      class_info->proxy, class_info->extends, class_info->constructor,
      class_info->properties, pos, end_pos,
      class_info->has_name_static_property,
      class_info->has_static_computed_names);

  do_block->statements()->Add(
      factory()->NewExpressionStatement(
          factory()->NewAssignment(Token::ASSIGN,
                                   factory()->NewVariableProxy(result_var),
                                   class_literal, kNoSourcePosition),
          pos),
      zone());
  do_block->set_scope(scope()->FinalizeBlockScope());
  do_expr->set_represented_function(class_info->constructor);
  AddFunctionForNameInference(class_info->constructor);

  return do_expr;
}

Literal* Parser::GetLiteralUndefined(int position) {
  return factory()->NewUndefinedLiteral(position);
}


void Parser::CheckConflictingVarDeclarations(Scope* scope, bool* ok) {
  Declaration* decl = scope->CheckConflictingVarDeclarations();
  if (decl != NULL) {
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


void Parser::InsertShadowingVarBindingInitializers(Block* inner_block) {
  // For each var-binding that shadows a parameter, insert an assignment
  // initializing the variable with the parameter.
  Scope* inner_scope = inner_block->scope();
  DCHECK(inner_scope->is_declaration_scope());
  Scope* function_scope = inner_scope->outer_scope();
  DCHECK(function_scope->is_function_scope());
  BlockState block_state(&scope_state_, inner_scope);
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
  for (ParserTarget* t = target_stack_; t != NULL; t = t->previous()) {
    if (ContainsLabel(t->statement()->labels(), label)) return true;
  }
  return false;
}


BreakableStatement* Parser::LookupBreakTarget(const AstRawString* label,
                                              bool* ok) {
  bool anonymous = label == NULL;
  for (ParserTarget* t = target_stack_; t != NULL; t = t->previous()) {
    BreakableStatement* stat = t->statement();
    if ((anonymous && stat->is_target_for_anonymous()) ||
        (!anonymous && ContainsLabel(stat->labels(), label))) {
      return stat;
    }
  }
  return NULL;
}


IterationStatement* Parser::LookupContinueTarget(const AstRawString* label,
                                                 bool* ok) {
  bool anonymous = label == NULL;
  for (ParserTarget* t = target_stack_; t != NULL; t = t->previous()) {
    IterationStatement* stat = t->statement()->AsIterationStatement();
    if (stat == NULL) continue;

    DCHECK(stat->is_target_for_anonymous());
    if (anonymous || ContainsLabel(stat->labels(), label)) {
      return stat;
    }
  }
  return NULL;
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


void Parser::Internalize(Isolate* isolate, Handle<Script> script, bool error) {
  // Internalize strings and values.
  ast_value_factory()->Internalize(isolate);

  // Error processing.
  if (error) {
    if (stack_overflow()) {
      isolate->StackOverflow();
    } else {
      DCHECK(pending_error_handler_.has_pending_error());
      pending_error_handler_.ThrowPendingError(isolate, script);
    }
  }

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
  if (!parsing_on_main_thread_ &&
      FLAG_runtime_stats ==
          v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE) {
    // Copy over the counters from the background thread to the main counters on
    // the isolate.
    isolate->counters()->runtime_call_stats()->Add(runtime_call_stats_);
  }
}

void Parser::ParseOnBackground(ParseInfo* info) {
  parsing_on_main_thread_ = false;

  DCHECK(info->literal() == NULL);
  FunctionLiteral* result = NULL;

  ParserLogger logger;
  if (produce_cached_parse_data()) {
    if (allow_lazy_) {
      log_ = &logger;
    } else {
      compile_options_ = ScriptCompiler::kNoCompileOptions;
    }
  }
  if (FLAG_runtime_stats) {
    // Create separate runtime stats for background parsing.
    runtime_call_stats_ = new (zone()) RuntimeCallStats();
  }

  std::unique_ptr<Utf16CharacterStream> stream;
  Utf16CharacterStream* stream_ptr;
  if (info->character_stream()) {
    DCHECK(info->source_stream() == nullptr);
    stream_ptr = info->character_stream();
  } else {
    DCHECK(info->character_stream() == nullptr);
    stream.reset(ScannerStream::For(info->source_stream(),
                                    info->source_stream_encoding(),
                                    runtime_call_stats_));
    stream_ptr = stream.get();
  }
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
    scanner_.Initialize(stream_ptr);
    result = DoParseProgram(info);
  } else {
    result = DoParseFunction(info, info->function_name(), stream_ptr);
  }

  info->set_literal(result);

  // We cannot internalize on a background thread; a foreground task will take
  // care of calling Parser::Internalize just before compilation.

  if (produce_cached_parse_data()) {
    if (result != NULL) *info->cached_data() = logger.GetScriptData();
    log_ = NULL;
  }
  if (FLAG_runtime_stats &
      v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
    auto value = v8::tracing::TracedValue::Create();
    runtime_call_stats_->Dump(value.get());
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"),
                         "V8.RuntimeStats", TRACE_EVENT_SCOPE_THREAD,
                         "runtime-call-stats", std::move(value));
  }
}

Parser::TemplateLiteralState Parser::OpenTemplateLiteral(int pos) {
  return new (zone()) TemplateLiteral(zone(), pos);
}


void Parser::AddTemplateSpan(TemplateLiteralState* state, bool tail) {
  int pos = scanner()->location().beg_pos;
  int end = scanner()->location().end_pos - (tail ? 1 : 2);
  const AstRawString* tv = scanner()->CurrentSymbol(ast_value_factory());
  const AstRawString* trv = scanner()->CurrentRawSymbol(ast_value_factory());
  Literal* cooked = factory()->NewStringLiteral(tv, pos);
  Literal* raw = factory()->NewStringLiteral(trv, pos);
  (*state)->AddTemplateSpan(cooked, raw, end, zone());
}


void Parser::AddTemplateExpression(TemplateLiteralState* state,
                                   Expression* expression) {
  (*state)->AddExpression(expression, zone());
}


Expression* Parser::CloseTemplateLiteral(TemplateLiteralState* state, int start,
                                         Expression* tag) {
  TemplateLiteral* lit = *state;
  int pos = lit->position();
  const ZoneList<Expression*>* cooked_strings = lit->cooked();
  const ZoneList<Expression*>* raw_strings = lit->raw();
  const ZoneList<Expression*>* expressions = lit->expressions();
  DCHECK_EQ(cooked_strings->length(), raw_strings->length());
  DCHECK_EQ(cooked_strings->length(), expressions->length() + 1);

  if (!tag) {
    // Build tree of BinaryOps to simplify code-generation
    Expression* expr = cooked_strings->at(0);
    int i = 0;
    while (i < expressions->length()) {
      Expression* sub = expressions->at(i++);
      Expression* cooked_str = cooked_strings->at(i);

      // Let middle be ToString(sub).
      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(sub, zone());
      Expression* middle = factory()->NewCallRuntime(Runtime::kInlineToString,
                                                     args, sub->position());

      expr = factory()->NewBinaryOperation(
          Token::ADD, factory()->NewBinaryOperation(
                          Token::ADD, expr, middle, expr->position()),
          cooked_str, sub->position());
    }
    return expr;
  } else {
    uint32_t hash = ComputeTemplateLiteralHash(lit);

    int cooked_idx = function_state_->NextMaterializedLiteralIndex();
    int raw_idx = function_state_->NextMaterializedLiteralIndex();

    // $getTemplateCallSite
    ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(4, zone());
    args->Add(factory()->NewArrayLiteral(
                  const_cast<ZoneList<Expression*>*>(cooked_strings),
                  cooked_idx, pos),
              zone());
    args->Add(
        factory()->NewArrayLiteral(
            const_cast<ZoneList<Expression*>*>(raw_strings), raw_idx, pos),
        zone());

    // Truncate hash to Smi-range.
    Smi* hash_obj = Smi::cast(Internals::IntToSmi(static_cast<int>(hash)));
    args->Add(factory()->NewNumberLiteral(hash_obj->value(), pos), zone());

    Expression* call_site = factory()->NewCallRuntime(
        Context::GET_TEMPLATE_CALL_SITE_INDEX, args, start);

    // Call TagFn
    ZoneList<Expression*>* call_args =
        new (zone()) ZoneList<Expression*>(expressions->length() + 1, zone());
    call_args->Add(call_site, zone());
    call_args->AddAll(*expressions, zone());
    return factory()->NewCall(tag, call_args, pos);
  }
}


uint32_t Parser::ComputeTemplateLiteralHash(const TemplateLiteral* lit) {
  const ZoneList<Expression*>* raw_strings = lit->raw();
  int total = raw_strings->length();
  DCHECK(total);

  uint32_t running_hash = 0;

  for (int index = 0; index < total; ++index) {
    if (index) {
      running_hash = StringHasher::ComputeRunningHashOneByte(
          running_hash, "${}", 3);
    }

    const AstRawString* raw_string =
        raw_strings->at(index)->AsLiteral()->raw_value()->AsString();
    if (raw_string->is_one_byte()) {
      const char* data = reinterpret_cast<const char*>(raw_string->raw_data());
      running_hash = StringHasher::ComputeRunningHashOneByte(
          running_hash, data, raw_string->length());
    } else {
      const uc16* data = reinterpret_cast<const uc16*>(raw_string->raw_data());
      running_hash = StringHasher::ComputeRunningHash(running_hash, data,
                                                      raw_string->length());
    }
  }

  return running_hash;
}

ZoneList<Expression*>* Parser::PrepareSpreadArguments(
    ZoneList<Expression*>* list) {
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(1, zone());
  if (list->length() == 1) {
    // Spread-call with single spread argument produces an InternalArray
    // containing the values from the array.
    //
    // Function is called or constructed with the produced array of arguments
    //
    // EG: Apply(Func, Spread(spread0))
    ZoneList<Expression*>* spread_list =
        new (zone()) ZoneList<Expression*>(0, zone());
    spread_list->Add(list->at(0)->AsSpread()->expression(), zone());
    args->Add(factory()->NewCallRuntime(Runtime::kSpreadIterablePrepare,
                                        spread_list, kNoSourcePosition),
              zone());
    return args;
  } else {
    // Spread-call with multiple arguments produces array literals for each
    // sequences of unspread arguments, and converts each spread iterable to
    // an Internal array. Finally, all of these produced arrays are flattened
    // into a single InternalArray, containing the arguments for the call.
    //
    // EG: Apply(Func, Flatten([unspread0, unspread1], Spread(spread0),
    //                         Spread(spread1), [unspread2, unspread3]))
    int i = 0;
    int n = list->length();
    while (i < n) {
      if (!list->at(i)->IsSpread()) {
        ZoneList<Expression*>* unspread =
            new (zone()) ZoneList<Expression*>(1, zone());

        // Push array of unspread parameters
        while (i < n && !list->at(i)->IsSpread()) {
          unspread->Add(list->at(i++), zone());
        }
        int literal_index = function_state_->NextMaterializedLiteralIndex();
        args->Add(factory()->NewArrayLiteral(unspread, literal_index,
                                             kNoSourcePosition),
                  zone());

        if (i == n) break;
      }

      // Push eagerly spread argument
      ZoneList<Expression*>* spread_list =
          new (zone()) ZoneList<Expression*>(1, zone());
      spread_list->Add(list->at(i++)->AsSpread()->expression(), zone());
      args->Add(factory()->NewCallRuntime(Context::SPREAD_ITERABLE_INDEX,
                                          spread_list, kNoSourcePosition),
                zone());
    }

    list = new (zone()) ZoneList<Expression*>(1, zone());
    list->Add(factory()->NewCallRuntime(Context::SPREAD_ARGUMENTS_INDEX, args,
                                        kNoSourcePosition),
              zone());
    return list;
  }
  UNREACHABLE();
}

Expression* Parser::SpreadCall(Expression* function,
                               ZoneList<Expression*>* args, int pos) {
  if (function->IsSuperCallReference()) {
    // Super calls
    // $super_constructor = %_GetSuperConstructor(<this-function>)
    // %reflect_construct($super_constructor, args, new.target)

    bool only_last_arg_is_spread = false;
    for (int i = 0; i < args->length(); i++) {
      if (args->at(i)->IsSpread()) {
        if (i == args->length() - 1) {
          only_last_arg_is_spread = true;
        }
        break;
      }
    }

    if (only_last_arg_is_spread) {
      // Handle in BytecodeGenerator.
      Expression* super_call_ref = NewSuperCallReference(pos);
      return factory()->NewCall(super_call_ref, args, pos);
    }
    args = PrepareSpreadArguments(args);
    ZoneList<Expression*>* tmp = new (zone()) ZoneList<Expression*>(1, zone());
    tmp->Add(function->AsSuperCallReference()->this_function_var(), zone());
    Expression* super_constructor = factory()->NewCallRuntime(
        Runtime::kInlineGetSuperConstructor, tmp, pos);
    args->InsertAt(0, super_constructor, zone());
    args->Add(function->AsSuperCallReference()->new_target_var(), zone());
    return factory()->NewCallRuntime(Context::REFLECT_CONSTRUCT_INDEX, args,
                                     pos);
  } else {
    args = PrepareSpreadArguments(args);
    if (function->IsProperty()) {
      // Method calls
      if (function->AsProperty()->IsSuperAccess()) {
        Expression* home = ThisExpression(kNoSourcePosition);
        args->InsertAt(0, function, zone());
        args->InsertAt(1, home, zone());
      } else {
        Variable* temp = NewTemporary(ast_value_factory()->empty_string());
        VariableProxy* obj = factory()->NewVariableProxy(temp);
        Assignment* assign_obj = factory()->NewAssignment(
            Token::ASSIGN, obj, function->AsProperty()->obj(),
            kNoSourcePosition);
        function = factory()->NewProperty(
            assign_obj, function->AsProperty()->key(), kNoSourcePosition);
        args->InsertAt(0, function, zone());
        obj = factory()->NewVariableProxy(temp);
        args->InsertAt(1, obj, zone());
      }
    } else {
      // Non-method calls
      args->InsertAt(0, function, zone());
      args->InsertAt(1, factory()->NewUndefinedLiteral(kNoSourcePosition),
                     zone());
    }
    return factory()->NewCallRuntime(Context::REFLECT_APPLY_INDEX, args, pos);
  }
}

Expression* Parser::SpreadCallNew(Expression* function,
                                  ZoneList<Expression*>* args, int pos) {
  args = PrepareSpreadArguments(args);
  args->InsertAt(0, function, zone());

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

void Parser::MarkCollectedTailCallExpressions() {
  const ZoneList<Expression*>& tail_call_expressions =
      function_state_->tail_call_expressions().expressions();
  for (int i = 0; i < tail_call_expressions.length(); ++i) {
    MarkTailPosition(tail_call_expressions[i]);
  }
}

Expression* Parser::ExpressionListToExpression(ZoneList<Expression*>* args) {
  Expression* expr = args->at(0);
  for (int i = 1; i < args->length(); ++i) {
    expr = factory()->NewBinaryOperation(Token::COMMA, expr, args->at(i),
                                         expr->position());
  }
  return expr;
}

// This method intoduces the line initializing the generator object
// when desugaring the body of async_function.
void Parser::PrepareAsyncFunctionBody(ZoneList<Statement*>* body,
                                      FunctionKind kind, int pos) {
  // When parsing an async arrow function, we get here without having called
  // PrepareGeneratorVariables yet, so do it now.
  if (function_state_->generator_object_variable() == nullptr) {
    PrepareGeneratorVariables();
  }
  body->Add(factory()->NewExpressionStatement(
                BuildCreateJSGeneratorObject(pos, kind), kNoSourcePosition),
            zone());
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
  block = BuildRejectPromiseOnException(block, CHECK_OK_VOID);
  body->Add(block, zone());
}

Expression* Parser::RewriteAwaitExpression(Expression* value, int await_pos) {
  // yield do {
  //   tmp = <operand>;
  //   %AsyncFunctionAwait(.generator_object, tmp, .promise);
  //   .promise
  // }
  // The value of the expression is returned to the caller of the async
  // function for the first yield statement; for this, .promise is the
  // appropriate return value, being a Promise that will be fulfilled or
  // rejected with the appropriate value by the desugaring. Subsequent yield
  // occurrences will return to the AsyncFunctionNext call within the
  // implemementation of the intermediate throwaway Promise's then handler.
  // This handler has nothing useful to do with the value, as the Promise is
  // ignored. If we yielded the value of the throwawayPromise that
  // AsyncFunctionAwait creates as an intermediate, it would create a memory
  // leak; we must return .promise instead;
  // The operand needs to be evaluated on a separate statement in order to get
  // a break location, and the .promise needs to be read earlier so that it
  // doesn't insert a false location.
  // TODO(littledan): investigate why this ordering is needed in more detail.
  Variable* generator_object_variable =
      function_state_->generator_object_variable();
  DCHECK_NOT_NULL(generator_object_variable);

  const int nopos = kNoSourcePosition;

  Block* do_block = factory()->NewBlock(nullptr, 2, false, nopos);

  Variable* promise = PromiseVariable();

  // Wrap value evaluation to provide a break location.
  Variable* temp_var = NewTemporary(ast_value_factory()->empty_string());
  Expression* value_assignment = factory()->NewAssignment(
      Token::ASSIGN, factory()->NewVariableProxy(temp_var), value, nopos);
  do_block->statements()->Add(
      factory()->NewExpressionStatement(value_assignment, value->position()),
      zone());

  ZoneList<Expression*>* async_function_await_args =
      new (zone()) ZoneList<Expression*>(3, zone());
  Expression* generator_object =
      factory()->NewVariableProxy(generator_object_variable);
  async_function_await_args->Add(generator_object, zone());
  async_function_await_args->Add(factory()->NewVariableProxy(temp_var), zone());
  async_function_await_args->Add(factory()->NewVariableProxy(promise), zone());

  // The parser emits calls to AsyncFunctionAwaitCaught, but the
  // AstNumberingVisitor will rewrite this to AsyncFunctionAwaitUncaught
  // if there is no local enclosing try/catch block.
  Expression* async_function_await =
      factory()->NewCallRuntime(Context::ASYNC_FUNCTION_AWAIT_CAUGHT_INDEX,
                                async_function_await_args, nopos);
  do_block->statements()->Add(
      factory()->NewExpressionStatement(async_function_await, await_pos),
      zone());

  // Wrap await to provide a break location between value evaluation and yield.
  Expression* do_expr = factory()->NewDoExpression(do_block, promise, nopos);

  generator_object = factory()->NewVariableProxy(generator_object_variable);
  return factory()->NewYield(generator_object, do_expr, nopos,
                             Yield::kOnExceptionRethrow);
}

class NonPatternRewriter : public AstExpressionRewriter {
 public:
  NonPatternRewriter(uintptr_t stack_limit, Parser* parser)
      : AstExpressionRewriter(stack_limit), parser_(parser) {}
  ~NonPatternRewriter() override {}

 private:
  bool RewriteExpression(Expression* expr) override {
    if (expr->IsRewritableExpression()) return true;
    // Rewrite only what could have been a pattern but is not.
    if (expr->IsArrayLiteral()) {
      // Spread rewriting in array literals.
      ArrayLiteral* lit = expr->AsArrayLiteral();
      VisitExpressions(lit->values());
      replacement_ = parser_->RewriteSpreads(lit);
      return false;
    }
    if (expr->IsObjectLiteral()) {
      return true;
    }
    if (expr->IsBinaryOperation() &&
        expr->AsBinaryOperation()->op() == Token::COMMA) {
      return true;
    }
    // Everything else does not need rewriting.
    return false;
  }

  void VisitLiteralProperty(LiteralProperty* property) override {
    if (property == nullptr) return;
    // Do not rewrite (computed) key expressions
    AST_REWRITE_PROPERTY(Expression, property, value);
  }

  Parser* parser_;
};

void Parser::RewriteNonPattern(bool* ok) {
  ValidateExpression(CHECK_OK_VOID);
  auto non_patterns_to_rewrite = function_state_->non_patterns_to_rewrite();
  int begin = classifier()->GetNonPatternBegin();
  int end = non_patterns_to_rewrite->length();
  if (begin < end) {
    NonPatternRewriter rewriter(stack_limit_, this);
    for (int i = begin; i < end; i++) {
      DCHECK(non_patterns_to_rewrite->at(i)->IsRewritableExpression());
      rewriter.Rewrite(non_patterns_to_rewrite->at(i));
    }
    non_patterns_to_rewrite->Rewind(begin);
  }
}


void Parser::RewriteDestructuringAssignments() {
  const auto& assignments =
      function_state_->destructuring_assignments_to_rewrite();
  for (int i = assignments.length() - 1; i >= 0; --i) {
    // Rewrite list in reverse, so that nested assignment patterns are rewritten
    // correctly.
    const DestructuringAssignment& pair = assignments.at(i);
    RewritableExpression* to_rewrite =
        pair.assignment->AsRewritableExpression();
    DCHECK_NOT_NULL(to_rewrite);
    if (!to_rewrite->is_rewritten()) {
      // Since this function is called at the end of parsing the program,
      // pair.scope may already have been removed by FinalizeBlockScope in the
      // meantime.
      Scope* scope = pair.scope->GetUnremovedScope();
      PatternRewriter::RewriteDestructuringAssignment(this, to_rewrite, scope);
    }
  }
}

Expression* Parser::RewriteExponentiation(Expression* left, Expression* right,
                                          int pos) {
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(left, zone());
  args->Add(right, zone());
  return factory()->NewCallRuntime(Context::MATH_POW_INDEX, args, pos);
}

Expression* Parser::RewriteAssignExponentiation(Expression* left,
                                                Expression* right, int pos) {
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  if (left->IsVariableProxy()) {
    VariableProxy* lhs = left->AsVariableProxy();

    Expression* result;
    DCHECK_NOT_NULL(lhs->raw_name());
    result = ExpressionFromIdentifier(lhs->raw_name(), lhs->position());
    args->Add(left, zone());
    args->Add(right, zone());
    Expression* call =
        factory()->NewCallRuntime(Context::MATH_POW_INDEX, args, pos);
    return factory()->NewAssignment(Token::ASSIGN, result, call, pos);
  } else if (left->IsProperty()) {
    Property* prop = left->AsProperty();
    auto temp_obj = NewTemporary(ast_value_factory()->empty_string());
    auto temp_key = NewTemporary(ast_value_factory()->empty_string());
    Expression* assign_obj = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(temp_obj), prop->obj(),
        kNoSourcePosition);
    Expression* assign_key = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(temp_key), prop->key(),
        kNoSourcePosition);
    args->Add(factory()->NewProperty(factory()->NewVariableProxy(temp_obj),
                                     factory()->NewVariableProxy(temp_key),
                                     left->position()),
              zone());
    args->Add(right, zone());
    Expression* call =
        factory()->NewCallRuntime(Context::MATH_POW_INDEX, args, pos);
    Expression* target = factory()->NewProperty(
        factory()->NewVariableProxy(temp_obj),
        factory()->NewVariableProxy(temp_key), kNoSourcePosition);
    Expression* assign =
        factory()->NewAssignment(Token::ASSIGN, target, call, pos);
    return factory()->NewBinaryOperation(
        Token::COMMA, assign_obj,
        factory()->NewBinaryOperation(Token::COMMA, assign_key, assign, pos),
        pos);
  }
  UNREACHABLE();
  return nullptr;
}

Expression* Parser::RewriteSpreads(ArrayLiteral* lit) {
  // Array literals containing spreads are rewritten using do expressions, e.g.
  //    [1, 2, 3, ...x, 4, ...y, 5]
  // is roughly rewritten as:
  //    do {
  //      $R = [1, 2, 3];
  //      for ($i of x) %AppendElement($R, $i);
  //      %AppendElement($R, 4);
  //      for ($j of y) %AppendElement($R, $j);
  //      %AppendElement($R, 5);
  //      $R
  //    }
  // where $R, $i and $j are fresh temporary variables.
  ZoneList<Expression*>::iterator s = lit->FirstSpread();
  if (s == lit->EndValue()) return nullptr;  // no spread, no rewriting...
  Variable* result = NewTemporary(ast_value_factory()->dot_result_string());
  // NOTE: The value assigned to R is the whole original array literal,
  // spreads included. This will be fixed before the rewritten AST is returned.
  // $R = lit
  Expression* init_result = factory()->NewAssignment(
      Token::INIT, factory()->NewVariableProxy(result), lit, kNoSourcePosition);
  Block* do_block = factory()->NewBlock(nullptr, 16, false, kNoSourcePosition);
  do_block->statements()->Add(
      factory()->NewExpressionStatement(init_result, kNoSourcePosition),
      zone());
  // Traverse the array literal starting from the first spread.
  while (s != lit->EndValue()) {
    Expression* value = *s++;
    Spread* spread = value->AsSpread();
    if (spread == nullptr) {
      // If the element is not a spread, we're adding a single:
      // %AppendElement($R, value)
      // or, in case of a hole,
      // ++($R.length)
      if (!value->IsLiteral() ||
          !value->AsLiteral()->raw_value()->IsTheHole()) {
        ZoneList<Expression*>* append_element_args = NewExpressionList(2);
        append_element_args->Add(factory()->NewVariableProxy(result), zone());
        append_element_args->Add(value, zone());
        do_block->statements()->Add(
            factory()->NewExpressionStatement(
                factory()->NewCallRuntime(Runtime::kAppendElement,
                                          append_element_args,
                                          kNoSourcePosition),
                kNoSourcePosition),
            zone());
      } else {
        Property* length_property = factory()->NewProperty(
            factory()->NewVariableProxy(result),
            factory()->NewStringLiteral(ast_value_factory()->length_string(),
                                        kNoSourcePosition),
            kNoSourcePosition);
        CountOperation* count_op = factory()->NewCountOperation(
            Token::INC, true /* prefix */, length_property, kNoSourcePosition);
        do_block->statements()->Add(
            factory()->NewExpressionStatement(count_op, kNoSourcePosition),
            zone());
      }
    } else {
      // If it's a spread, we're adding a for/of loop iterating through it.
      Variable* each = NewTemporary(ast_value_factory()->dot_for_string());
      Expression* subject = spread->expression();
      // %AppendElement($R, each)
      Statement* append_body;
      {
        ZoneList<Expression*>* append_element_args = NewExpressionList(2);
        append_element_args->Add(factory()->NewVariableProxy(result), zone());
        append_element_args->Add(factory()->NewVariableProxy(each), zone());
        append_body = factory()->NewExpressionStatement(
            factory()->NewCallRuntime(Runtime::kAppendElement,
                                      append_element_args, kNoSourcePosition),
            kNoSourcePosition);
      }
      // for (each of spread) %AppendElement($R, each)
      ForEachStatement* loop = factory()->NewForEachStatement(
          ForEachStatement::ITERATE, nullptr, kNoSourcePosition);
      const bool finalize = false;
      InitializeForOfStatement(loop->AsForOfStatement(),
                               factory()->NewVariableProxy(each), subject,
                               append_body, finalize);
      do_block->statements()->Add(loop, zone());
    }
  }
  // Now, rewind the original array literal to truncate everything from the
  // first spread (included) until the end. This fixes $R's initialization.
  lit->RewindSpreads();
  return factory()->NewDoExpression(do_block, result, lit->position());
}

void Parser::QueueDestructuringAssignmentForRewriting(Expression* expr) {
  DCHECK(expr->IsRewritableExpression());
  function_state_->AddDestructuringAssignment(
      DestructuringAssignment(expr, scope()));
}

void Parser::QueueNonPatternForRewriting(Expression* expr, bool* ok) {
  DCHECK(expr->IsRewritableExpression());
  function_state_->AddNonPatternForRewriting(expr, ok);
}

void Parser::AddAccessorPrefixToFunctionName(bool is_get,
                                             FunctionLiteral* function,
                                             const AstRawString* name) {
  DCHECK_NOT_NULL(name);
  const AstRawString* prefix = is_get ? ast_value_factory()->get_space_string()
                                      : ast_value_factory()->set_space_string();
  function->set_raw_name(ast_value_factory()->NewConsString(prefix, name));
}

void Parser::SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                             const AstRawString* name) {
  DCHECK(property->kind() != ObjectLiteralProperty::GETTER);
  DCHECK(property->kind() != ObjectLiteralProperty::SETTER);

  // Computed name setting must happen at runtime.
  DCHECK(!property->is_computed_name());

  // Ignore "__proto__" as a name when it's being used to set the [[Prototype]]
  // of an object literal.
  if (property->kind() == ObjectLiteralProperty::PROTOTYPE) return;

  Expression* value = property->value();

  DCHECK(!value->IsAnonymousFunctionDefinition() ||
         property->kind() == ObjectLiteralProperty::COMPUTED);
  SetFunctionName(value, name);
}

void Parser::SetFunctionNameFromIdentifierRef(Expression* value,
                                              Expression* identifier) {
  if (!identifier->IsVariableProxy()) return;
  SetFunctionName(value, identifier->AsVariableProxy()->raw_name());
}

void Parser::SetFunctionName(Expression* value, const AstRawString* name) {
  DCHECK_NOT_NULL(name);
  if (!value->IsAnonymousFunctionDefinition()) return;
  auto function = value->AsFunctionLiteral();
  if (function != nullptr) {
    function->set_raw_name(name);
  } else {
    DCHECK(value->IsDoExpression());
    value->AsDoExpression()->represented_function()->set_raw_name(name);
  }
}


// Desugaring of yield*
// ====================
//
// With the help of do-expressions and function.sent, we desugar yield* into a
// loop containing a "raw" yield (a yield that doesn't wrap an iterator result
// object around its argument).  Concretely, "yield* iterable" turns into
// roughly the following code:
//
//   do {
//     const kNext = 0;
//     const kReturn = 1;
//     const kThrow = 2;
//
//     let input = function.sent;
//     let mode = kNext;
//     let output = undefined;
//
//     let iterator = GetIterator(iterable);
//
//     while (true) {
//       // From the generator to the iterator:
//       // Forward input according to resume mode and obtain output.
//       switch (mode) {
//         case kNext:
//           output = iterator.next(input);
//           if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
//           break;
//         case kReturn:
//           IteratorClose(iterator, input, output);  // See below.
//           break;
//         case kThrow:
//           let iteratorThrow = iterator.throw;
//           if (IS_NULL_OR_UNDEFINED(iteratorThrow)) {
//             IteratorClose(iterator);  // See below.
//             throw MakeTypeError(kThrowMethodMissing);
//           }
//           output = %_Call(iteratorThrow, iterator, input);
//           if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
//           break;
//       }
//       if (output.done) break;
//
//       // From the generator to its user:
//       // Forward output, receive new input, and determine resume mode.
//       mode = kReturn;
//       try {
//         try {
//           RawYield(output);  // See explanation above.
//           mode = kNext;
//         } catch (error) {
//           mode = kThrow;
//         }
//       } finally {
//         input = function.sent;
//         continue;
//       }
//     }
//
//     if (mode === kReturn) {
//       return {value: output.value, done: true};
//     }
//     output.value
//   }
//
// IteratorClose(iterator) expands to the following:
//
//   let iteratorReturn = iterator.return;
//   if (!IS_NULL_OR_UNDEFINED(iteratorReturn)) {
//     let output = %_Call(iteratorReturn, iterator);
//     if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
//   }
//
// IteratorClose(iterator, input, output) expands to the following:
//
//   let iteratorReturn = iterator.return;
//   if (IS_NULL_OR_UNDEFINED(iteratorReturn)) return input;
//   output = %_Call(iteratorReturn, iterator, input);
//   if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);

Expression* Parser::RewriteYieldStar(Expression* generator,
                                     Expression* iterable, int pos) {
  const int nopos = kNoSourcePosition;

  // Forward definition for break/continue statements.
  WhileStatement* loop = factory()->NewWhileStatement(nullptr, nopos);

  // let input = undefined;
  Variable* var_input = NewTemporary(ast_value_factory()->empty_string());
  Statement* initialize_input;
  {
    Expression* input_proxy = factory()->NewVariableProxy(var_input);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, input_proxy,
                                 factory()->NewUndefinedLiteral(nopos), nopos);
    initialize_input = factory()->NewExpressionStatement(assignment, nopos);
  }

  // let mode = kNext;
  Variable* var_mode = NewTemporary(ast_value_factory()->empty_string());
  Statement* initialize_mode;
  {
    Expression* mode_proxy = factory()->NewVariableProxy(var_mode);
    Expression* knext =
        factory()->NewSmiLiteral(JSGeneratorObject::kNext, nopos);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, mode_proxy, knext, nopos);
    initialize_mode = factory()->NewExpressionStatement(assignment, nopos);
  }

  // let output = undefined;
  Variable* var_output = NewTemporary(ast_value_factory()->empty_string());
  Statement* initialize_output;
  {
    Expression* output_proxy = factory()->NewVariableProxy(var_output);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, output_proxy,
                                 factory()->NewUndefinedLiteral(nopos), nopos);
    initialize_output = factory()->NewExpressionStatement(assignment, nopos);
  }

  // let iterator = GetIterator(iterable);
  Variable* var_iterator = NewTemporary(ast_value_factory()->empty_string());
  Statement* get_iterator;
  {
    Expression* iterator = factory()->NewGetIterator(iterable, nopos);
    Expression* iterator_proxy = factory()->NewVariableProxy(var_iterator);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, iterator_proxy, iterator, nopos);
    get_iterator = factory()->NewExpressionStatement(assignment, nopos);
  }

  // output = iterator.next(input);
  Statement* call_next;
  {
    Expression* iterator_proxy = factory()->NewVariableProxy(var_iterator);
    Expression* literal =
        factory()->NewStringLiteral(ast_value_factory()->next_string(), nopos);
    Expression* next_property =
        factory()->NewProperty(iterator_proxy, literal, nopos);
    Expression* input_proxy = factory()->NewVariableProxy(var_input);
    auto args = new (zone()) ZoneList<Expression*>(1, zone());
    args->Add(input_proxy, zone());
    Expression* call = factory()->NewCall(next_property, args, nopos);
    Expression* output_proxy = factory()->NewVariableProxy(var_output);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, output_proxy, call, nopos);
    call_next = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
  Statement* validate_next_output;
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

    validate_next_output = factory()->NewIfStatement(
        is_receiver_call, factory()->NewEmptyStatement(nopos), throw_call,
        nopos);
  }

  // let iteratorThrow = iterator.throw;
  Variable* var_throw = NewTemporary(ast_value_factory()->empty_string());
  Statement* get_throw;
  {
    Expression* iterator_proxy = factory()->NewVariableProxy(var_iterator);
    Expression* literal =
        factory()->NewStringLiteral(ast_value_factory()->throw_string(), nopos);
    Expression* property =
        factory()->NewProperty(iterator_proxy, literal, nopos);
    Expression* throw_proxy = factory()->NewVariableProxy(var_throw);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, throw_proxy, property, nopos);
    get_throw = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (IS_NULL_OR_UNDEFINED(iteratorThrow) {
  //   IteratorClose(iterator);
  //   throw MakeTypeError(kThrowMethodMissing);
  // }
  Statement* check_throw;
  {
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ, factory()->NewVariableProxy(var_throw),
        factory()->NewNullLiteral(nopos), nopos);
    Expression* call =
        NewThrowTypeError(MessageTemplate::kThrowMethodMissing,
                          ast_value_factory()->empty_string(), nopos);
    Statement* throw_call = factory()->NewExpressionStatement(call, nopos);

    Block* then = factory()->NewBlock(nullptr, 4 + 1, false, nopos);
    BuildIteratorCloseForCompletion(
        scope(), then->statements(), var_iterator,
        factory()->NewSmiLiteral(Parser::kNormalCompletion, nopos));
    then->statements()->Add(throw_call, zone());
    check_throw = factory()->NewIfStatement(
        condition, then, factory()->NewEmptyStatement(nopos), nopos);
  }

  // output = %_Call(iteratorThrow, iterator, input);
  Statement* call_throw;
  {
    auto args = new (zone()) ZoneList<Expression*>(3, zone());
    args->Add(factory()->NewVariableProxy(var_throw), zone());
    args->Add(factory()->NewVariableProxy(var_iterator), zone());
    args->Add(factory()->NewVariableProxy(var_input), zone());
    Expression* call =
        factory()->NewCallRuntime(Runtime::kInlineCall, args, nopos);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(var_output), call, nopos);
    call_throw = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
  Statement* validate_throw_output;
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

    validate_throw_output = factory()->NewIfStatement(
        is_receiver_call, factory()->NewEmptyStatement(nopos), throw_call,
        nopos);
  }

  // if (output.done) break;
  Statement* if_done;
  {
    Expression* output_proxy = factory()->NewVariableProxy(var_output);
    Expression* literal =
        factory()->NewStringLiteral(ast_value_factory()->done_string(), nopos);
    Expression* property = factory()->NewProperty(output_proxy, literal, nopos);
    BreakStatement* break_loop = factory()->NewBreakStatement(loop, nopos);
    if_done = factory()->NewIfStatement(
        property, break_loop, factory()->NewEmptyStatement(nopos), nopos);
  }


  // mode = kReturn;
  Statement* set_mode_return;
  {
    Expression* mode_proxy = factory()->NewVariableProxy(var_mode);
    Expression* kreturn =
        factory()->NewSmiLiteral(JSGeneratorObject::kReturn, nopos);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, mode_proxy, kreturn, nopos);
    set_mode_return = factory()->NewExpressionStatement(assignment, nopos);
  }

  // Yield(output);
  Statement* yield_output;
  {
    Expression* output_proxy = factory()->NewVariableProxy(var_output);
    Yield* yield = factory()->NewYield(generator, output_proxy, nopos,
                                       Yield::kOnExceptionThrow);
    yield_output = factory()->NewExpressionStatement(yield, nopos);
  }

  // mode = kNext;
  Statement* set_mode_next;
  {
    Expression* mode_proxy = factory()->NewVariableProxy(var_mode);
    Expression* knext =
        factory()->NewSmiLiteral(JSGeneratorObject::kNext, nopos);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, mode_proxy, knext, nopos);
    set_mode_next = factory()->NewExpressionStatement(assignment, nopos);
  }

  // mode = kThrow;
  Statement* set_mode_throw;
  {
    Expression* mode_proxy = factory()->NewVariableProxy(var_mode);
    Expression* kthrow =
        factory()->NewSmiLiteral(JSGeneratorObject::kThrow, nopos);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, mode_proxy, kthrow, nopos);
    set_mode_throw = factory()->NewExpressionStatement(assignment, nopos);
  }

  // input = function.sent;
  Statement* get_input;
  {
    Expression* function_sent = FunctionSentExpression(nopos);
    Expression* input_proxy = factory()->NewVariableProxy(var_input);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, input_proxy, function_sent, nopos);
    get_input = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (mode === kReturn) {
  //   return {value: output.value, done: true};
  // }
  Statement* maybe_return_value;
  {
    Expression* mode_proxy = factory()->NewVariableProxy(var_mode);
    Expression* kreturn =
        factory()->NewSmiLiteral(JSGeneratorObject::kReturn, nopos);
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ_STRICT, mode_proxy, kreturn, nopos);

    Expression* output_proxy = factory()->NewVariableProxy(var_output);
    Expression* literal =
        factory()->NewStringLiteral(ast_value_factory()->value_string(), nopos);
    Expression* property = factory()->NewProperty(output_proxy, literal, nopos);
    Statement* return_value = factory()->NewReturnStatement(
        BuildIteratorResult(property, true), nopos);

    maybe_return_value = factory()->NewIfStatement(
        condition, return_value, factory()->NewEmptyStatement(nopos), nopos);
  }

  // output.value
  Statement* get_value;
  {
    Expression* output_proxy = factory()->NewVariableProxy(var_output);
    Expression* literal =
        factory()->NewStringLiteral(ast_value_factory()->value_string(), nopos);
    Expression* property = factory()->NewProperty(output_proxy, literal, nopos);
    get_value = factory()->NewExpressionStatement(property, nopos);
  }

  // Now put things together.

  // try { ... } catch(e) { ... }
  Statement* try_catch;
  {
    Block* try_block = factory()->NewBlock(nullptr, 2, false, nopos);
    try_block->statements()->Add(yield_output, zone());
    try_block->statements()->Add(set_mode_next, zone());

    Block* catch_block = factory()->NewBlock(nullptr, 1, false, nopos);
    catch_block->statements()->Add(set_mode_throw, zone());

    Scope* catch_scope = NewScope(CATCH_SCOPE);
    catch_scope->set_is_hidden();
    const AstRawString* name = ast_value_factory()->dot_catch_string();
    Variable* catch_variable = catch_scope->DeclareLocal(name, VAR);

    try_catch = factory()->NewTryCatchStatementForDesugaring(
        try_block, catch_scope, catch_variable, catch_block, nopos);
  }

  // try { ... } finally { ... }
  Statement* try_finally;
  {
    Block* try_block = factory()->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(try_catch, zone());

    Block* finally = factory()->NewBlock(nullptr, 2, false, nopos);
    finally->statements()->Add(get_input, zone());
    finally->statements()->Add(factory()->NewContinueStatement(loop, nopos),
                               zone());

    try_finally = factory()->NewTryFinallyStatement(try_block, finally, nopos);
  }

  // switch (mode) { ... }
  SwitchStatement* switch_mode = factory()->NewSwitchStatement(nullptr, nopos);
  {
    auto case_next = new (zone()) ZoneList<Statement*>(3, zone());
    case_next->Add(call_next, zone());
    case_next->Add(validate_next_output, zone());
    case_next->Add(factory()->NewBreakStatement(switch_mode, nopos), zone());

    auto case_return = new (zone()) ZoneList<Statement*>(5, zone());
    BuildIteratorClose(case_return, var_iterator, var_input, var_output);
    case_return->Add(factory()->NewBreakStatement(switch_mode, nopos), zone());

    auto case_throw = new (zone()) ZoneList<Statement*>(5, zone());
    case_throw->Add(get_throw, zone());
    case_throw->Add(check_throw, zone());
    case_throw->Add(call_throw, zone());
    case_throw->Add(validate_throw_output, zone());
    case_throw->Add(factory()->NewBreakStatement(switch_mode, nopos), zone());

    auto cases = new (zone()) ZoneList<CaseClause*>(3, zone());
    Expression* knext =
        factory()->NewSmiLiteral(JSGeneratorObject::kNext, nopos);
    Expression* kreturn =
        factory()->NewSmiLiteral(JSGeneratorObject::kReturn, nopos);
    Expression* kthrow =
        factory()->NewSmiLiteral(JSGeneratorObject::kThrow, nopos);
    cases->Add(factory()->NewCaseClause(knext, case_next, nopos), zone());
    cases->Add(factory()->NewCaseClause(kreturn, case_return, nopos), zone());
    cases->Add(factory()->NewCaseClause(kthrow, case_throw, nopos), zone());

    switch_mode->Initialize(factory()->NewVariableProxy(var_mode), cases);
  }

  // while (true) { ... }
  // Already defined earlier: WhileStatement* loop = ...
  {
    Block* loop_body = factory()->NewBlock(nullptr, 4, false, nopos);
    loop_body->statements()->Add(switch_mode, zone());
    loop_body->statements()->Add(if_done, zone());
    loop_body->statements()->Add(set_mode_return, zone());
    loop_body->statements()->Add(try_finally, zone());

    loop->Initialize(factory()->NewBooleanLiteral(true, nopos), loop_body);
  }

  // do { ... }
  DoExpression* yield_star;
  {
    // The rewriter needs to process the get_value statement only, hence we
    // put the preceding statements into an init block.

    Block* do_block_ = factory()->NewBlock(nullptr, 6, true, nopos);
    do_block_->statements()->Add(initialize_input, zone());
    do_block_->statements()->Add(initialize_mode, zone());
    do_block_->statements()->Add(initialize_output, zone());
    do_block_->statements()->Add(get_iterator, zone());
    do_block_->statements()->Add(loop, zone());
    do_block_->statements()->Add(maybe_return_value, zone());

    Block* do_block = factory()->NewBlock(nullptr, 2, false, nopos);
    do_block->statements()->Add(do_block_, zone());
    do_block->statements()->Add(get_value, zone());

    Variable* dot_result =
        NewTemporary(ast_value_factory()->dot_result_string());
    yield_star = factory()->NewDoExpression(do_block, dot_result, nopos);
    Rewriter::Rewrite(this, GetClosureScope(), yield_star, ast_value_factory());
  }

  return yield_star;
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
                                Variable* var_output) {
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

    Statement* return_input =
        factory()->NewReturnStatement(BuildIteratorResult(value, true), nopos);

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

void Parser::FinalizeIteratorUse(Scope* use_scope, Variable* completion,
                                 Expression* condition, Variable* iter,
                                 Block* iterator_use, Block* target) {
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
    Block* block = factory()->NewBlock(nullptr, 2, true, nopos);
    Expression* proxy = factory()->NewVariableProxy(completion);
    BuildIteratorCloseForCompletion(use_scope, block->statements(), iter,
                                    proxy);
    DCHECK(block->statements()->length() == 2);

    maybe_close = factory()->NewBlock(nullptr, 1, true, nopos);
    maybe_close->statements()->Add(
        factory()->NewIfStatement(condition, block,
                                  factory()->NewEmptyStatement(nopos), nopos),
        zone());
  }

  // try { #try_block }
  // catch(e) {
  //   #set_completion_throw;
  //   %ReThrow(e);
  // }
  Statement* try_catch;
  {
    Scope* catch_scope = NewScopeWithParent(use_scope, CATCH_SCOPE);
    Variable* catch_variable =
        catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(), VAR);
    catch_scope->set_is_hidden();

    Statement* rethrow;
    // We use %ReThrow rather than the ordinary throw because we want to
    // preserve the original exception message.  This is also why we create a
    // TryCatchStatementForReThrow below (which does not clear the pending
    // message), rather than a TryCatchStatement.
    {
      auto args = new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(factory()->NewVariableProxy(catch_variable), zone());
      rethrow = factory()->NewExpressionStatement(
          factory()->NewCallRuntime(Runtime::kReThrow, args, nopos), nopos);
    }

    Block* catch_block = factory()->NewBlock(nullptr, 2, false, nopos);
    catch_block->statements()->Add(set_completion_throw, zone());
    catch_block->statements()->Add(rethrow, zone());

    try_catch = factory()->NewTryCatchStatementForReThrow(
        iterator_use, catch_scope, catch_variable, catch_block, nopos);
  }

  // try { #try_catch } finally { #maybe_close }
  Statement* try_finally;
  {
    Block* try_block = factory()->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(try_catch, zone());

    try_finally =
        factory()->NewTryFinallyStatement(try_block, maybe_close, nopos);
  }

  target->statements()->Add(initialize_completion, zone());
  target->statements()->Add(try_finally, zone());
}

void Parser::BuildIteratorCloseForCompletion(Scope* scope,
                                             ZoneList<Statement*>* statements,
                                             Variable* iterator,
                                             Expression* completion) {
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
  //       try { %_Call(iteratorReturn, iterator) } catch (_) { }
  //     } else {
  //       let output = %_Call(iteratorReturn, iterator);
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

    Block* try_block = factory()->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(factory()->NewExpressionStatement(call, nopos),
                                 zone());

    Block* catch_block = factory()->NewBlock(nullptr, 0, false, nopos);

    Scope* catch_scope = NewScopeWithParent(scope, CATCH_SCOPE);
    Variable* catch_variable =
        catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(), VAR);
    catch_scope->set_is_hidden();

    try_call_return = factory()->NewTryCatchStatement(
        try_block, catch_scope, catch_variable, catch_block, nopos);
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

    validate_return = factory()->NewBlock(nullptr, 2, false, nopos);
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

    Block* then_block = factory()->NewBlock(nullptr, 2, false, nopos);
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
                                          Variable* var_completion, int pos) {
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

  Block* final_loop = factory()->NewBlock(nullptr, 2, false, nopos);
  {
    Block* try_block = factory()->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(loop, zone());

    // The scope in which the parser creates this loop.
    Scope* loop_scope = scope()->outer_scope();
    DCHECK_EQ(loop_scope->scope_type(), BLOCK_SCOPE);
    DCHECK_EQ(scope()->scope_type(), BLOCK_SCOPE);

    FinalizeIteratorUse(loop_scope, var_completion, closing_condition,
                        loop->iterator(), try_block, final_loop);
  }

  return final_loop;
}

#undef CHECK_OK
#undef CHECK_OK_VOID
#undef CHECK_FAILED

}  // namespace internal
}  // namespace v8
