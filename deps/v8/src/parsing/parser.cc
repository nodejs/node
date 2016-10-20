// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parser.h"

#include <memory>

#include "src/api.h"
#include "src/ast/ast-expression-rewriter.h"
#include "src/ast/ast-literal-reindexer.h"
#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/ast.h"
#include "src/bailout-reason.h"
#include "src/base/platform/platform.h"
#include "src/char-predicates-inl.h"
#include "src/messages.h"
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
  if (HasError()) return false;
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


bool ParseData::HasError() {
  return Data()[PreparseDataConstants::kHasErrorOffset];
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
        prev_zone_(parser->zone_) {
    if (use_temp_zone) {
      parser_->fni_ = &fni_;
      parser_->zone_ = temp_zone;
    }
  }
  ~DiscardableZoneScope() {
    parser_->fni_ = prev_fni_;
    parser_->zone_ = prev_zone_;
  }

 private:
  AstNodeFactory::BodyScope ast_node_factory_scope_;
  FuncNameInferrer fni_;
  Parser* parser_;
  FuncNameInferrer* prev_fni_;
  Zone* prev_zone_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableZoneScope);
};

void Parser::SetCachedData(ParseInfo* info) {
  if (compile_options_ == ScriptCompiler::kNoCompileOptions) {
    cached_parse_data_ = NULL;
  } else {
    DCHECK(info->cached_data() != NULL);
    if (compile_options_ == ScriptCompiler::kConsumeParserCache) {
      cached_parse_data_ = ParseData::FromCachedData(*info->cached_data());
    }
  }
}

FunctionLiteral* Parser::DefaultConstructor(const AstRawString* name,
                                            bool call_super, int pos,
                                            int end_pos,
                                            LanguageMode language_mode) {
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  int parameter_count = 0;
  if (name == nullptr) name = ast_value_factory()->empty_string();

  FunctionKind kind = call_super ? FunctionKind::kDefaultSubclassConstructor
                                 : FunctionKind::kDefaultBaseConstructor;
  DeclarationScope* function_scope = NewFunctionScope(kind);
  SetLanguageMode(function_scope,
                  static_cast<LanguageMode>(language_mode | STRICT));
  // Set start and end position to the same value
  function_scope->set_start_position(pos);
  function_scope->set_end_position(pos);
  ZoneList<Statement*>* body = NULL;

  {
    FunctionState function_state(&function_state_, &scope_state_,
                                 function_scope, kind);

    body = new (zone()) ZoneList<Statement*>(call_super ? 2 : 1, zone());
    if (call_super) {
      // $super_constructor = %_GetSuperConstructor(<this-function>)
      // %reflect_construct(
      //     $super_constructor, InternalArray(...args), new.target)
      auto constructor_args_name = ast_value_factory()->empty_string();
      bool is_duplicate;
      bool is_rest = true;
      bool is_optional = false;
      Variable* constructor_args = function_scope->DeclareParameter(
          constructor_args_name, TEMPORARY, is_optional, is_rest, &is_duplicate,
          ast_value_factory());

      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(2, zone());
      VariableProxy* this_function_proxy =
          NewUnresolved(ast_value_factory()->this_function_string(), pos);
      ZoneList<Expression*>* tmp =
          new (zone()) ZoneList<Expression*>(1, zone());
      tmp->Add(this_function_proxy, zone());
      Expression* super_constructor = factory()->NewCallRuntime(
          Runtime::kInlineGetSuperConstructor, tmp, pos);
      args->Add(super_constructor, zone());
      Spread* spread_args = factory()->NewSpread(
          factory()->NewVariableProxy(constructor_args), pos, pos);
      ZoneList<Expression*>* spread_args_expr =
          new (zone()) ZoneList<Expression*>(1, zone());
      spread_args_expr->Add(spread_args, zone());
      args->AddAll(*PrepareSpreadArguments(spread_args_expr), zone());
      VariableProxy* new_target_proxy =
          NewUnresolved(ast_value_factory()->new_target_string(), pos);
      args->Add(new_target_proxy, zone());
      CallRuntime* call = factory()->NewCallRuntime(
          Context::REFLECT_CONSTRUCT_INDEX, args, pos);
      body->Add(factory()->NewReturnStatement(call, pos), zone());
    }

    materialized_literal_count = function_state.materialized_literal_count();
    expected_property_count = function_state.expected_property_count();
  }

  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      name, function_scope, body, materialized_literal_count,
      expected_property_count, parameter_count,
      FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::kAnonymousExpression,
      FunctionLiteral::kShouldLazyCompile, kind, pos);

  return function_literal;
}


// ----------------------------------------------------------------------------
// Target is a support class to facilitate manipulation of the
// Parser's target_stack_ (the stack of potential 'break' and
// 'continue' statement targets). Upon construction, a new target is
// added; it is removed upon destruction.

class Target BASE_EMBEDDED {
 public:
  Target(Target** variable, BreakableStatement* statement)
      : variable_(variable), statement_(statement), previous_(*variable) {
    *variable = this;
  }

  ~Target() {
    *variable_ = previous_;
  }

  Target* previous() { return previous_; }
  BreakableStatement* statement() { return statement_; }

 private:
  Target** variable_;
  BreakableStatement* statement_;
  Target* previous_;
};


class TargetScope BASE_EMBEDDED {
 public:
  explicit TargetScope(Target** variable)
      : variable_(variable), previous_(*variable) {
    *variable = NULL;
  }

  ~TargetScope() {
    *variable_ = previous_;
  }

 private:
  Target** variable_;
  Target* previous_;
};


// ----------------------------------------------------------------------------
// The CHECK_OK macro is a convenient macro to enforce error
// handling for functions that may fail (by returning !*ok).
//
// CAUTION: This macro appends extra statements after a call,
// thus it must never be used where only a single statement
// is correct (e.g. an if statement branch w/o braces)!

#define CHECK_OK  ok);      \
  if (!*ok) return nullptr; \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

#define CHECK_OK_VOID  ok); \
  if (!*ok) return;         \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

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
  static const int kNewTargetStringLength = 10;
  auto proxy = NewUnresolved(ast_value_factory()->new_target_string(), pos,
                             pos + kNewTargetStringLength);
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
      int value = scanner()->smi_value();
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

Expression* Parser::GetIterator(Expression* iterable, int pos) {
  Expression* iterator_symbol_literal =
      factory()->NewSymbolLiteral("iterator_symbol", kNoSourcePosition);
  Expression* prop =
      factory()->NewProperty(iterable, iterator_symbol_literal, pos);
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(0, zone());
  return factory()->NewCall(prop, args, pos);
}

void Parser::MarkTailPosition(Expression* expression) {
  expression->MarkTail();
}

Parser::Parser(ParseInfo* info)
    : ParserBase<Parser>(info->zone(), &scanner_, info->stack_limit(),
                         info->extension(), info->ast_value_factory(), NULL),
      scanner_(info->unicode_cache()),
      reusable_preparser_(NULL),
      original_scope_(NULL),
      target_stack_(NULL),
      compile_options_(info->compile_options()),
      cached_parse_data_(NULL),
      total_preparse_skipped_(0),
      pre_parse_timer_(NULL),
      parsing_on_main_thread_(true) {
  // Even though we were passed ParseInfo, we should not store it in
  // Parser - this makes sure that Isolate is not accidentally accessed via
  // ParseInfo during background parsing.
  DCHECK(!info->script().is_null() || info->source_stream() != nullptr ||
         info->character_stream() != nullptr);
  set_allow_lazy(info->allow_lazy_parsing());
  set_allow_natives(FLAG_allow_natives_syntax || info->is_native());
  set_allow_tailcalls(FLAG_harmony_tailcalls && !info->is_native() &&
                      info->isolate()->is_tail_call_elimination_enabled());
  set_allow_harmony_do_expressions(FLAG_harmony_do_expressions);
  set_allow_harmony_for_in(FLAG_harmony_for_in);
  set_allow_harmony_function_sent(FLAG_harmony_function_sent);
  set_allow_harmony_restrictive_declarations(
      FLAG_harmony_restrictive_declarations);
  set_allow_harmony_async_await(FLAG_harmony_async_await);
  set_allow_harmony_restrictive_generators(FLAG_harmony_restrictive_generators);
  set_allow_harmony_trailing_commas(FLAG_harmony_trailing_commas);
  for (int feature = 0; feature < v8::Isolate::kUseCounterFeatureCount;
       ++feature) {
    use_counts_[feature] = 0;
  }
  if (info->ast_value_factory() == NULL) {
    // info takes ownership of AstValueFactory.
    info->set_ast_value_factory(new AstValueFactory(zone(), info->hash_seed()));
    info->set_ast_value_factory_owned();
    ast_value_factory_ = info->ast_value_factory();
    ast_node_factory_.set_ast_value_factory(ast_value_factory_);
  }
}

void Parser::DeserializeScopeChain(
    ParseInfo* info, Handle<Context> context,
    Scope::DeserializationMode deserialization_mode) {
  DCHECK(ThreadId::Current().Equals(info->isolate()->thread_id()));
  // TODO(wingo): Add an outer SCRIPT_SCOPE corresponding to the native
  // context, which will have the "this" binding for script scopes.
  DeclarationScope* script_scope = NewScriptScope();
  info->set_script_scope(script_scope);
  Scope* scope = script_scope;
  if (!context.is_null() && !context->IsNativeContext()) {
    scope = Scope::DeserializeScopeChain(info->isolate(), zone(), *context,
                                         script_scope, ast_value_factory(),
                                         deserialization_mode);
    if (info->context().is_null()) {
      DCHECK(deserialization_mode ==
             Scope::DeserializationMode::kDeserializeOffHeap);
    } else {
      // The Scope is backed up by ScopeInfo (which is in the V8 heap); this
      // means the Parser cannot operate independent of the V8 heap. Tell the
      // string table to internalize strings and values right after they're
      // created. This kind of parsing can only be done in the main thread.
      DCHECK(parsing_on_main_thread_);
      ast_value_factory()->Internalize(info->isolate());
    }
  }
  original_scope_ = scope;
}

FunctionLiteral* Parser::ParseProgram(Isolate* isolate, ParseInfo* info) {
  // TODO(bmeurer): We temporarily need to pass allow_nesting = true here,
  // see comment for HistogramTimerScope class.

  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);

  HistogramTimerScope timer_scope(isolate->counters()->parse(), true);
  RuntimeCallTimerScope runtime_timer(isolate, &RuntimeCallStats::Parse);
  TRACE_EVENT_RUNTIME_CALL_STATS_TRACING_SCOPED(
      isolate, &tracing::TraceEventStatsTable::Parse);
  Handle<String> source(String::cast(info->script()->source()));
  isolate->counters()->total_parse_size()->Increment(source->length());
  base::ElapsedTimer timer;
  if (FLAG_trace_parse) {
    timer.Start();
  }
  fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());

  // Initialize parser state.
  CompleteParserRecorder recorder;

  if (produce_cached_parse_data()) {
    log_ = &recorder;
  } else if (consume_cached_parse_data()) {
    cached_parse_data_->Initialize();
  }

  DeserializeScopeChain(info, info->context(),
                        Scope::DeserializationMode::kKeepScopeInfo);

  source = String::Flatten(source);
  FunctionLiteral* result;

  {
    std::unique_ptr<Utf16CharacterStream> stream;
    if (source->IsExternalTwoByteString()) {
      stream.reset(new ExternalTwoByteStringUtf16CharacterStream(
          Handle<ExternalTwoByteString>::cast(source), 0, source->length()));
    } else if (source->IsExternalOneByteString()) {
      stream.reset(new ExternalOneByteStringUtf16CharacterStream(
          Handle<ExternalOneByteString>::cast(source), 0, source->length()));
    } else {
      stream.reset(
          new GenericStringUtf16CharacterStream(source, 0, source->length()));
    }
    scanner_.Initialize(stream.get());
    result = DoParseProgram(info);
  }
  if (result != NULL) {
    DCHECK_EQ(scanner_.peek_location().beg_pos, source->length());
  }
  HandleSourceURLComments(isolate, info->script());

  if (FLAG_trace_parse && result != NULL) {
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
  if (produce_cached_parse_data()) {
    if (result != NULL) *info->cached_data() = recorder.GetScriptData();
    log_ = NULL;
  }
  return result;
}


FunctionLiteral* Parser::DoParseProgram(ParseInfo* info) {
  // Note that this function can be called from the main thread or from a
  // background thread. We should not access anything Isolate / heap dependent
  // via ParseInfo, and also not pass it forward.
  DCHECK_NULL(scope_state_);
  DCHECK_NULL(target_stack_);

  Mode parsing_mode = FLAG_lazy && allow_lazy() ? PARSE_LAZILY : PARSE_EAGERLY;
  if (allow_natives() || extension_ != NULL) parsing_mode = PARSE_EAGERLY;

  FunctionLiteral* result = NULL;
  {
    Scope* outer = original_scope_;
    // If there's a chance that there's a reference to global 'this', predeclare
    // it as a dynamic global on the script scope.
    if (outer->GetReceiverScope()->is_script_scope()) {
      info->script_scope()->DeclareDynamicGlobal(
          ast_value_factory()->this_string(), Variable::THIS);
    }
    DCHECK(outer);
    if (info->is_eval()) {
      if (!outer->is_script_scope() || is_strict(info->language_mode())) {
        parsing_mode = PARSE_EAGERLY;
      }
      outer = NewEvalScope(outer);
    } else if (info->is_module()) {
      DCHECK_EQ(outer, info->script_scope());
      outer = NewModuleScope(info->script_scope());
    }

    DeclarationScope* scope = outer->AsDeclarationScope();

    scope->set_start_position(0);

    // Enter 'scope' with the given parsing mode.
    ParsingModeScope parsing_mode_scope(this, parsing_mode);
    FunctionState function_state(&function_state_, &scope_state_, scope,
                                 kNormalFunction);

    ZoneList<Statement*>* body = new(zone()) ZoneList<Statement*>(16, zone());
    bool ok = true;
    int beg_pos = scanner()->location().beg_pos;
    parsing_module_ = info->is_module();
    if (parsing_module_) {
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
      CheckDecimalLiteralWithLeadingZero(use_counts_, beg_pos,
                                         scanner()->location().end_pos);
    }
    if (ok && is_sloppy(language_mode())) {
      // TODO(littledan): Function bindings on the global object that modify
      // pre-existing bindings should be made writable, enumerable and
      // nonconfigurable if possible, whereas this code will leave attributes
      // unchanged if the property already exists.
      InsertSloppyBlockFunctionVarBindings(scope, nullptr, &ok);
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
      result = factory()->NewScriptOrEvalFunctionLiteral(
          scope, body, function_state.materialized_literal_count(),
          function_state.expected_property_count());
    }
  }

  // Make sure the target stack is empty.
  DCHECK(target_stack_ == NULL);

  return result;
}


FunctionLiteral* Parser::ParseLazy(Isolate* isolate, ParseInfo* info) {
  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);
  RuntimeCallTimerScope runtime_timer(isolate, &RuntimeCallStats::ParseLazy);
  HistogramTimerScope timer_scope(isolate->counters()->parse_lazy());
  TRACE_EVENT_RUNTIME_CALL_STATS_TRACING_SCOPED(
      isolate, &tracing::TraceEventStatsTable::ParseLazy);
  Handle<String> source(String::cast(info->script()->source()));
  isolate->counters()->total_parse_size()->Increment(source->length());
  base::ElapsedTimer timer;
  if (FLAG_trace_parse) {
    timer.Start();
  }
  Handle<SharedFunctionInfo> shared_info = info->shared_info();
  DeserializeScopeChain(info, info->context(),
                        Scope::DeserializationMode::kKeepScopeInfo);

  // Initialize parser state.
  source = String::Flatten(source);
  FunctionLiteral* result;
  {
    std::unique_ptr<Utf16CharacterStream> stream;
    if (source->IsExternalTwoByteString()) {
      stream.reset(new ExternalTwoByteStringUtf16CharacterStream(
          Handle<ExternalTwoByteString>::cast(source),
          shared_info->start_position(), shared_info->end_position()));
    } else if (source->IsExternalOneByteString()) {
      stream.reset(new ExternalOneByteStringUtf16CharacterStream(
          Handle<ExternalOneByteString>::cast(source),
          shared_info->start_position(), shared_info->end_position()));
    } else {
      stream.reset(new GenericStringUtf16CharacterStream(
          source, shared_info->start_position(), shared_info->end_position()));
    }
    Handle<String> name(String::cast(shared_info->name()));
    result =
        DoParseLazy(info, ast_value_factory()->GetString(name), stream.get());
    if (result != nullptr) {
      Handle<String> inferred_name(shared_info->inferred_name());
      result->set_inferred_name(inferred_name);
    }
  }

  if (FLAG_trace_parse && result != NULL) {
    double ms = timer.Elapsed().InMillisecondsF();
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

FunctionLiteral* Parser::DoParseLazy(ParseInfo* info,
                                     const AstRawString* raw_name,
                                     Utf16CharacterStream* source) {
  scanner_.Initialize(source);
  DCHECK_NULL(scope_state_);
  DCHECK_NULL(target_stack_);

  DCHECK(ast_value_factory());
  fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());
  fni_->PushEnclosingName(raw_name);

  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  // Place holder for the result.
  FunctionLiteral* result = nullptr;

  {
    // Parse the function literal.
    Scope* scope = original_scope_;
    DCHECK(scope);
    // If there's a chance that there's a reference to global 'this', predeclare
    // it as a dynamic global on the script scope.
    if (info->is_arrow() && scope->GetReceiverScope()->is_script_scope()) {
      info->script_scope()->DeclareDynamicGlobal(
          ast_value_factory()->this_string(), Variable::THIS);
    }
    FunctionState function_state(&function_state_, &scope_state_, scope,
                                 info->function_kind());
    DCHECK(is_sloppy(scope->language_mode()) ||
           is_strict(info->language_mode()));
    FunctionLiteral::FunctionType function_type = ComputeFunctionType(info);
    bool ok = true;

    if (info->is_arrow()) {
      bool is_async = allow_harmony_async_await() && info->is_async();
      if (is_async) {
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
      DeclarationScope* scope = NewFunctionScope(FunctionKind::kArrowFunction);

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
          ParseFormalParameterList(&formals, &formals_classifier, &ok);
          if (ok) ok = Check(Token::RPAREN);
        } else {
          // BindingIdentifier
          ParseFormalParameter(&formals, &formals_classifier, &ok);
          if (ok) {
            DeclareFormalParameter(formals.scope, formals.at(0),
                                   &formals_classifier);
          }
        }
      }

      if (ok) {
        checkpoint.Restore(&formals.materialized_literals_count);
        // Pass `accept_IN=true` to ParseArrowFunctionLiteral --- This should
        // not be observable, or else the preparser would have failed.
        Expression* expression = ParseArrowFunctionLiteral(
            true, formals, is_async, formals_classifier, &ok);
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
    } else if (info->is_default_constructor()) {
      DCHECK_EQ(this->scope(), scope);
      result = DefaultConstructor(
          raw_name, IsSubclassConstructor(info->function_kind()),
          info->start_position(), info->end_position(), info->language_mode());
    } else {
      result = ParseFunctionLiteral(raw_name, Scanner::Location::invalid(),
                                    kSkipFunctionNameCheck,
                                    info->function_kind(), kNoSourcePosition,
                                    function_type, info->language_mode(), &ok);
    }
    // Make sure the results agree.
    DCHECK(ok == (result != nullptr));
  }

  // Make sure the target stack is empty.
  DCHECK_NULL(target_stack_);
  return result;
}


void Parser::ParseStatementList(ZoneList<Statement*>* body, int end_token,
                                bool* ok) {
  // StatementList ::
  //   (StatementListItem)* <end_token>

  // Allocate a target stack to use for this set of source
  // elements. This way, all scripts and functions get their own
  // target stack thus avoiding illegal breaks and continues across
  // functions.
  TargetScope scope(&this->target_stack_);

  DCHECK(body != NULL);
  bool directive_prologue = true;     // Parsing directive prologue.

  while (peek() != end_token) {
    if (directive_prologue && peek() != Token::STRING) {
      directive_prologue = false;
    }

    Scanner::Location token_loc = scanner()->peek_location();
    Statement* stat = ParseStatementListItem(CHECK_OK_VOID);
    if (stat == NULL || stat->IsEmpty()) {
      directive_prologue = false;   // End of directive prologue.
      continue;
    }

    if (directive_prologue) {
      // A shot at a directive.
      ExpressionStatement* e_stat;
      Literal* literal;
      // Still processing directive prologue?
      if ((e_stat = stat->AsExpressionStatement()) != NULL &&
          (literal = e_stat->expression()->AsLiteral()) != NULL &&
          literal->raw_value()->IsString()) {
        // Check "use strict" directive (ES5 14.1), "use asm" directive.
        bool use_strict_found =
            literal->raw_value()->AsString() ==
                ast_value_factory()->use_strict_string() &&
            token_loc.end_pos - token_loc.beg_pos ==
                ast_value_factory()->use_strict_string()->length() + 2;
        if (use_strict_found) {
          if (is_sloppy(language_mode())) {
            RaiseLanguageMode(STRICT);
          }

          if (!this->scope()->HasSimpleParameters()) {
            // TC39 deemed "use strict" directives to be an error when occurring
            // in the body of a function with non-simple parameter list, on
            // 29/7/2015. https://goo.gl/ueA7Ln
            const AstRawString* string = literal->raw_value()->AsString();
            ReportMessageAt(token_loc,
                            MessageTemplate::kIllegalLanguageModeDirective,
                            string);
            *ok = false;
            return;
          }
          // Because declarations in strict eval code don't leak into the scope
          // of the eval call, it is likely that functions declared in strict
          // eval code will be used within the eval code, so lazy parsing is
          // probably not a win.
          if (this->scope()->is_eval_scope()) mode_ = PARSE_EAGERLY;
        } else if (literal->raw_value()->AsString() ==
                       ast_value_factory()->use_asm_string() &&
                   token_loc.end_pos - token_loc.beg_pos ==
                       ast_value_factory()->use_asm_string()->length() + 2) {
          // Store the usage count; The actual use counter on the isolate is
          // incremented after parsing is done.
          ++use_counts_[v8::Isolate::kUseAsm];
          DCHECK(this->scope()->is_declaration_scope());
          this->scope()->AsDeclarationScope()->set_asm_module();
        } else {
          // Should not change mode, but will increment UseCounter
          // if appropriate. Ditto usages below.
          RaiseLanguageMode(SLOPPY);
        }
      } else {
        // End of the directive prologue.
        directive_prologue = false;
        RaiseLanguageMode(SLOPPY);
      }
    } else {
      RaiseLanguageMode(SLOPPY);
    }

    body->Add(stat, zone());
  }
}


Statement* Parser::ParseStatementListItem(bool* ok) {
  // (Ecma 262 6th Edition, 13.1):
  // StatementListItem:
  //    Statement
  //    Declaration
  const Token::Value peeked = peek();
  switch (peeked) {
    case Token::FUNCTION:
      return ParseHoistableDeclaration(NULL, false, ok);
    case Token::CLASS:
      Consume(Token::CLASS);
      return ParseClassDeclaration(NULL, false, ok);
    case Token::CONST:
      return ParseVariableStatement(kStatementListItem, NULL, ok);
    case Token::VAR:
      return ParseVariableStatement(kStatementListItem, NULL, ok);
    case Token::LET:
      if (IsNextLetKeyword()) {
        return ParseVariableStatement(kStatementListItem, NULL, ok);
      }
      break;
    case Token::ASYNC:
      if (allow_harmony_async_await() && PeekAhead() == Token::FUNCTION &&
          !scanner()->HasAnyLineTerminatorAfterNext()) {
        Consume(Token::ASYNC);
        return ParseAsyncFunctionDeclaration(NULL, false, ok);
      }
    /* falls through */
    default:
      break;
  }
  return ParseStatement(NULL, kAllowLabelledFunctionStatement, ok);
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
    if (CheckContextualKeyword(CStrVector("as"))) {
      export_name = ParseIdentifierName(CHECK_OK_VOID);
    }
    if (export_name == NULL) {
      export_name = local_name;
    }
    export_names->Add(export_name, zone());
    local_names->Add(local_name, zone());
    export_locations->Add(scanner()->location(), zone());
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

    NamedImport* import = new (zone()) NamedImport(
        import_name, local_name, scanner()->location());
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
    module()->AddEmptyImport(module_specifier, scanner()->location(), zone());
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
      module()->AddEmptyImport(module_specifier, scanner()->location(), zone());
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
      if (allow_harmony_async_await() && PeekAhead() == Token::FUNCTION &&
          !scanner()->HasAnyLineTerminatorAfterNext()) {
        Consume(Token::ASYNC);
        result = ParseAsyncFunctionDeclaration(&local_names, true, CHECK_OK);
        break;
      }
    /* falls through */

    default: {
      int pos = position();
      ExpressionClassifier classifier(this);
      Expression* value =
          ParseAssignmentExpression(true, &classifier, CHECK_OK);
      RewriteNonPattern(&classifier, CHECK_OK);
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

  int pos = peek_position();
  Expect(Token::EXPORT, CHECK_OK);

  Statement* result = nullptr;
  ZoneList<const AstRawString*> names(1, zone());
  switch (peek()) {
    case Token::DEFAULT:
      return ParseExportDefault(ok);

    case Token::MUL: {
      Consume(Token::MUL);
      ExpectContextualKeyword(CStrVector("from"), CHECK_OK);
      const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK);
      ExpectSemicolon(CHECK_OK);
      module()->AddStarExport(module_specifier, scanner()->location(), zone());
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
        module()->AddEmptyImport(module_specifier, scanner()->location(),
                                 zone());
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
      if (allow_harmony_async_await()) {
        // TODO(neis): Why don't we have the same check here as in
        // ParseStatementListItem?
        Consume(Token::ASYNC);
        result = ParseAsyncFunctionDeclaration(&names, false, CHECK_OK);
        break;
      }
    /* falls through */

    default:
      *ok = false;
      ReportUnexpectedToken(scanner()->current_token());
      return nullptr;
  }

  ModuleDescriptor* descriptor = module();
  for (int i = 0; i < names.length(); ++i) {
    // TODO(neis): Provide better location.
    descriptor->AddExport(names[i], names[i], scanner()->location(), zone());
  }

  DCHECK_NOT_NULL(result);
  return result;
}

Statement* Parser::ParseStatement(ZoneList<const AstRawString*>* labels,
                                  AllowLabelledFunctionStatement allow_function,
                                  bool* ok) {
  // Statement ::
  //   EmptyStatement
  //   ...

  if (peek() == Token::SEMICOLON) {
    Next();
    return factory()->NewEmptyStatement(kNoSourcePosition);
  }
  return ParseSubStatement(labels, allow_function, ok);
}

Statement* Parser::ParseSubStatement(
    ZoneList<const AstRawString*>* labels,
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
  switch (peek()) {
    case Token::LBRACE:
      return ParseBlock(labels, ok);

    case Token::SEMICOLON:
      Next();
      return factory()->NewEmptyStatement(kNoSourcePosition);

    case Token::IF:
      return ParseIfStatement(labels, ok);

    case Token::DO:
      return ParseDoWhileStatement(labels, ok);

    case Token::WHILE:
      return ParseWhileStatement(labels, ok);

    case Token::FOR:
      return ParseForStatement(labels, ok);

    case Token::CONTINUE:
    case Token::BREAK:
    case Token::RETURN:
    case Token::THROW:
    case Token::TRY: {
      // These statements must have their labels preserved in an enclosing
      // block
      if (labels == NULL) {
        return ParseStatementAsUnlabelled(labels, ok);
      } else {
        Block* result =
            factory()->NewBlock(labels, 1, false, kNoSourcePosition);
        Target target(&this->target_stack_, result);
        Statement* statement = ParseStatementAsUnlabelled(labels, CHECK_OK);
        if (result) result->statements()->Add(statement, zone());
        return result;
      }
    }

    case Token::WITH:
      return ParseWithStatement(labels, ok);

    case Token::SWITCH:
      return ParseSwitchStatement(labels, ok);

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
      return nullptr;

    case Token::DEBUGGER:
      return ParseDebuggerStatement(ok);

    case Token::VAR:
      return ParseVariableStatement(kStatement, NULL, ok);

    default:
      return ParseExpressionOrLabelledStatement(labels, allow_function, ok);
  }
}

Statement* Parser::ParseStatementAsUnlabelled(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  switch (peek()) {
    case Token::CONTINUE:
      return ParseContinueStatement(ok);

    case Token::BREAK:
      return ParseBreakStatement(labels, ok);

    case Token::RETURN:
      return ParseReturnStatement(ok);

    case Token::THROW:
      return ParseThrowStatement(ok);

    case Token::TRY:
      return ParseTryStatement(ok);

    default:
      UNREACHABLE();
      return NULL;
  }
}

VariableProxy* Parser::NewUnresolved(const AstRawString* name, int begin_pos,
                                     int end_pos, Variable::Kind kind) {
  return scope()->NewUnresolved(factory(), name, begin_pos, end_pos, kind);
}

VariableProxy* Parser::NewUnresolved(const AstRawString* name) {
  return scope()->NewUnresolved(factory(), name, scanner()->location().beg_pos,
                                scanner()->location().end_pos);
}

InitializationFlag Parser::DefaultInitializationFlag(VariableMode mode) {
  DCHECK(IsDeclaredVariableMode(mode));
  return mode == VAR ? kCreatedInitialized : kNeedsInitialization;
}

Declaration* Parser::DeclareVariable(const AstRawString* name,
                                     VariableMode mode, int pos, bool* ok) {
  return DeclareVariable(name, mode, DefaultInitializationFlag(mode), pos, ok);
}

Declaration* Parser::DeclareVariable(const AstRawString* name,
                                     VariableMode mode, InitializationFlag init,
                                     int pos, bool* ok) {
  DCHECK_NOT_NULL(name);
  Scope* scope =
      IsLexicalVariableMode(mode) ? this->scope() : GetDeclarationScope();
  VariableProxy* proxy =
      scope->NewUnresolved(factory(), name, scanner()->location().beg_pos,
                           scanner()->location().end_pos);
  Declaration* declaration =
      factory()->NewVariableDeclaration(proxy, this->scope(), pos);
  Declare(declaration, DeclarationDescriptor::NORMAL, mode, init, CHECK_OK);
  return declaration;
}

Variable* Parser::Declare(Declaration* declaration,
                          DeclarationDescriptor::Kind declaration_kind,
                          VariableMode mode, InitializationFlag init, bool* ok,
                          Scope* scope) {
  DCHECK(IsDeclaredVariableMode(mode) && mode != CONST_LEGACY);

  VariableProxy* proxy = declaration->proxy();
  DCHECK(proxy->raw_name() != NULL);
  const AstRawString* name = proxy->raw_name();

  if (scope == nullptr) scope = this->scope();
  if (mode == VAR) scope = scope->GetDeclarationScope();
  DCHECK(!scope->is_catch_scope());
  DCHECK(!scope->is_with_scope());
  DCHECK(scope->is_declaration_scope() ||
         (IsLexicalVariableMode(mode) && scope->is_block_scope()));

  bool is_function_declaration = declaration->IsFunctionDeclaration();

  Variable* var = NULL;
  if (scope->is_eval_scope() && is_sloppy(scope->language_mode()) &&
      mode == VAR) {
    // In a var binding in a sloppy direct eval, pollute the enclosing scope
    // with this new binding by doing the following:
    // The proxy is bound to a lookup variable to force a dynamic declaration
    // using the DeclareEvalVar or DeclareEvalFunction runtime functions.
    Variable::Kind kind = Variable::NORMAL;
    // TODO(sigurds) figure out if kNotAssigned is OK here
    var = new (zone()) Variable(scope, name, mode, kind, init, kNotAssigned);
    var->AllocateTo(VariableLocation::LOOKUP, -1);
  } else {
    // Declare the variable in the declaration scope.
    var = scope->LookupLocal(name);
    if (var == NULL) {
      // Declare the name.
      Variable::Kind kind = Variable::NORMAL;
      if (is_function_declaration) {
        kind = Variable::FUNCTION;
      }
      var = scope->DeclareLocal(name, mode, init, kind, kNotAssigned);
    } else if (IsLexicalVariableMode(mode) ||
               IsLexicalVariableMode(var->mode())) {
      // Allow duplicate function decls for web compat, see bug 4693.
      bool duplicate_allowed = false;
      if (is_sloppy(scope->language_mode()) && is_function_declaration &&
          var->is_function()) {
        DCHECK(IsLexicalVariableMode(mode) &&
               IsLexicalVariableMode(var->mode()));
        // If the duplication is allowed, then the var will show up
        // in the SloppyBlockFunctionMap and the new FunctionKind
        // will be a permitted duplicate.
        FunctionKind function_kind =
            declaration->AsFunctionDeclaration()->fun()->kind();
        duplicate_allowed =
            scope->GetDeclarationScope()->sloppy_block_function_map()->Lookup(
                const_cast<AstRawString*>(name), name->hash()) != nullptr &&
            !IsAsyncFunction(function_kind) &&
            !(allow_harmony_restrictive_generators() &&
              IsGeneratorFunction(function_kind));
      }
      if (duplicate_allowed) {
        ++use_counts_[v8::Isolate::kSloppyModeBlockScopedFunctionRedefinition];
      } else {
        // The name was declared in this scope before; check for conflicting
        // re-declarations. We have a conflict if either of the declarations
        // is not a var (in script scope, we also have to ignore legacy const
        // for compatibility). There is similar code in runtime.cc in the
        // Declare functions. The function CheckConflictingVarDeclarations
        // checks for var and let bindings from different scopes whereas this
        // is a check for conflicting declarations within the same scope. This
        // check also covers the special case
        //
        // function () { let x; { var x; } }
        //
        // because the var declaration is hoisted to the function scope where
        // 'x' is already bound.
        DCHECK(IsDeclaredVariableMode(var->mode()));
        // In harmony we treat re-declarations as early errors. See
        // ES5 16 for a definition of early errors.
        if (declaration_kind == DeclarationDescriptor::NORMAL) {
          ReportMessage(MessageTemplate::kVarRedeclaration, name);
        } else {
          ReportMessage(MessageTemplate::kParamDupe);
        }
        *ok = false;
        return nullptr;
      }
    } else if (mode == VAR) {
      var->set_maybe_assigned();
    }
  }
  DCHECK_NOT_NULL(var);

  // We add a declaration node for every declaration. The compiler
  // will only generate code if necessary. In particular, declarations
  // for inner local variables that do not represent functions won't
  // result in any generated code.
  //
  // This will lead to multiple declaration nodes for the
  // same variable if it is declared several times. This is not a
  // semantic issue, but it may be a performance issue since it may
  // lead to repeated DeclareEvalVar or DeclareEvalFunction calls.
  scope->AddDeclaration(declaration);
  proxy->BindTo(var);
  return var;
}


// Language extension which is only enabled for source files loaded
// through the API's extension mechanism.  A native function
// declaration is resolved by looking up the function through a
// callback provided by the extension.
Statement* Parser::ParseNativeDeclaration(bool* ok) {
  int pos = peek_position();
  Expect(Token::FUNCTION, CHECK_OK);
  // Allow "eval" or "arguments" for backward compatibility.
  const AstRawString* name =
      ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  bool done = (peek() == Token::RPAREN);
  while (!done) {
    ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
    done = (peek() == Token::RPAREN);
    if (!done) {
      Expect(Token::COMMA, CHECK_OK);
    }
  }
  Expect(Token::RPAREN, CHECK_OK);
  Expect(Token::SEMICOLON, CHECK_OK);

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

Statement* Parser::ParseHoistableDeclaration(
    ZoneList<const AstRawString*>* names, bool default_export, bool* ok) {
  Expect(Token::FUNCTION, CHECK_OK);
  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlags::kIsNormal;
  if (Check(Token::MUL)) {
    flags |= ParseFunctionFlags::kIsGenerator;
  }
  return ParseHoistableDeclaration(pos, flags, names, default_export, ok);
}

Statement* Parser::ParseAsyncFunctionDeclaration(
    ZoneList<const AstRawString*>* names, bool default_export, bool* ok) {
  DCHECK_EQ(scanner()->current_token(), Token::ASYNC);
  int pos = position();
  if (scanner()->HasAnyLineTerminatorBeforeNext()) {
    *ok = false;
    ReportUnexpectedToken(scanner()->current_token());
    return nullptr;
  }
  Expect(Token::FUNCTION, CHECK_OK);
  ParseFunctionFlags flags = ParseFunctionFlags::kIsAsync;
  return ParseHoistableDeclaration(pos, flags, names, default_export, ok);
}

Statement* Parser::ParseHoistableDeclaration(
    int pos, ParseFunctionFlags flags, ZoneList<const AstRawString*>* names,
    bool default_export, bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  //   'function' '(' FormalParameters ')' '{' FunctionBody '}'
  // GeneratorDeclaration ::
  //   'function' '*' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  //   'function' '*' '(' FormalParameters ')' '{' FunctionBody '}'
  //
  // The anonymous forms are allowed iff [default_export] is true.
  //
  // 'function' and '*' (if present) have been consumed by the caller.

  const bool is_generator = flags & ParseFunctionFlags::kIsGenerator;
  const bool is_async = flags & ParseFunctionFlags::kIsAsync;
  DCHECK(!is_generator || !is_async);

  const AstRawString* name;
  FunctionNameValidity name_validity;
  const AstRawString* variable_name;
  if (default_export && peek() == Token::LPAREN) {
    name = ast_value_factory()->default_string();
    name_validity = kSkipFunctionNameCheck;
    variable_name = ast_value_factory()->star_default_star_string();
  } else {
    bool is_strict_reserved;
    name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved, CHECK_OK);
    name_validity = is_strict_reserved ? kFunctionNameIsStrictReserved
                                       : kFunctionNameValidityUnknown;
    variable_name = name;
  }

  FuncNameInferrer::State fni_state(fni_);
  if (fni_ != NULL) fni_->PushEnclosingName(name);
  FunctionLiteral* fun = ParseFunctionLiteral(
      name, scanner()->location(), name_validity,
      is_generator ? FunctionKind::kGeneratorFunction
                   : is_async ? FunctionKind::kAsyncFunction
                              : FunctionKind::kNormalFunction,
      pos, FunctionLiteral::kDeclaration, language_mode(), CHECK_OK);

  // In ES6, a function behaves as a lexical binding, except in
  // a script scope, or the initial scope of eval or another function.
  VariableMode mode =
      (!scope()->is_declaration_scope() || scope()->is_module_scope()) ? LET
                                                                       : VAR;
  VariableProxy* proxy = NewUnresolved(variable_name);
  Declaration* declaration =
      factory()->NewFunctionDeclaration(proxy, fun, scope(), pos);
  Declare(declaration, DeclarationDescriptor::NORMAL, mode, kCreatedInitialized,
          CHECK_OK);
  if (names) names->Add(variable_name, zone());
  EmptyStatement* empty = factory()->NewEmptyStatement(kNoSourcePosition);
  // Async functions don't undergo sloppy mode block scoped hoisting, and don't
  // allow duplicates in a block. Both are represented by the
  // sloppy_block_function_map. Don't add them to the map for async functions.
  // Generators are also supposed to be prohibited; currently doing this behind
  // a flag and UseCounting violations to assess web compatibility.
  if (is_sloppy(language_mode()) && !scope()->is_declaration_scope() &&
      !is_async && !(allow_harmony_restrictive_generators() && is_generator)) {
    SloppyBlockFunctionStatement* delegate =
        factory()->NewSloppyBlockFunctionStatement(empty, scope());
    DeclarationScope* target_scope = GetDeclarationScope();
    target_scope->DeclareSloppyBlockFunction(variable_name, delegate);
    return delegate;
  }
  return empty;
}

Statement* Parser::ParseClassDeclaration(ZoneList<const AstRawString*>* names,
                                         bool default_export, bool* ok) {
  // ClassDeclaration ::
  //   'class' Identifier ('extends' LeftHandExpression)? '{' ClassBody '}'
  //   'class' ('extends' LeftHandExpression)? '{' ClassBody '}'
  //
  // The anonymous form is allowed iff [default_export] is true.
  //
  // 'class' is expected to be consumed by the caller.
  //
  // A ClassDeclaration
  //
  //   class C { ... }
  //
  // has the same semantics as:
  //
  //   let C = class C { ... };
  //
  // so rewrite it as such.

  int pos = position();

  const AstRawString* name;
  bool is_strict_reserved;
  const AstRawString* variable_name;
  if (default_export && (peek() == Token::EXTENDS || peek() == Token::LBRACE)) {
    name = ast_value_factory()->default_string();
    is_strict_reserved = false;
    variable_name = ast_value_factory()->star_default_star_string();
  } else {
    name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved, CHECK_OK);
    variable_name = name;
  }

  Expression* value = ParseClassLiteral(nullptr, name, scanner()->location(),
                                        is_strict_reserved, pos, CHECK_OK);

  Declaration* decl = DeclareVariable(variable_name, LET, pos, CHECK_OK);
  decl->proxy()->var()->set_initializer_position(position());
  Assignment* assignment =
      factory()->NewAssignment(Token::INIT, decl->proxy(), value, pos);
  Statement* assignment_statement =
      factory()->NewExpressionStatement(assignment, kNoSourcePosition);
  if (names) names->Add(variable_name, zone());
  return assignment_statement;
}

Block* Parser::ParseBlock(ZoneList<const AstRawString*>* labels, bool* ok) {
  // The harmony mode uses block elements instead of statements.
  //
  // Block ::
  //   '{' StatementList '}'

  // Construct block expecting 16 statements.
  Block* body = factory()->NewBlock(labels, 16, false, kNoSourcePosition);

  // Parse the statements and collect escaping labels.
  Expect(Token::LBRACE, CHECK_OK);
  {
    BlockState block_state(&scope_state_);
    block_state.set_start_position(scanner()->location().beg_pos);
    Target target(&this->target_stack_, body);

    while (peek() != Token::RBRACE) {
      Statement* stat = ParseStatementListItem(CHECK_OK);
      if (stat && !stat->IsEmpty()) {
        body->statements()->Add(stat, zone());
      }
    }

    Expect(Token::RBRACE, CHECK_OK);
    block_state.set_end_position(scanner()->location().end_pos);
    body->set_scope(block_state.FinalizedBlockScope());
  }
  return body;
}


Block* Parser::DeclarationParsingResult::BuildInitializationBlock(
    ZoneList<const AstRawString*>* names, bool* ok) {
  Block* result = descriptor.parser->factory()->NewBlock(
      NULL, 1, true, descriptor.declaration_pos);
  for (auto declaration : declarations) {
    PatternRewriter::DeclareAndInitializeVariables(
        result, &descriptor, &declaration, names, CHECK_OK);
  }
  return result;
}


Block* Parser::ParseVariableStatement(VariableDeclarationContext var_context,
                                      ZoneList<const AstRawString*>* names,
                                      bool* ok) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  // The scope of a var declared variable anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). Thus we can
  // transform a source-level var declaration into a (Function) Scope
  // declaration, and rewrite the source-level initialization into an assignment
  // statement. We use a block to collect multiple assignments.
  //
  // We mark the block as initializer block because we don't want the
  // rewriter to add a '.result' assignment to such a block (to get compliant
  // behavior for code such as print(eval('var x = 7')), and for cosmetic
  // reasons when pretty-printing. Also, unless an assignment (initialization)
  // is inside an initializer block, it is ignored.

  DeclarationParsingResult parsing_result;
  Block* result =
      ParseVariableDeclarations(var_context, &parsing_result, names, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return result;
}

Block* Parser::ParseVariableDeclarations(
    VariableDeclarationContext var_context,
    DeclarationParsingResult* parsing_result,
    ZoneList<const AstRawString*>* names, bool* ok) {
  // VariableDeclarations ::
  //   ('var' | 'const' | 'let') (Identifier ('=' AssignmentExpression)?)+[',']
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

  parsing_result->descriptor.parser = this;
  parsing_result->descriptor.declaration_kind = DeclarationDescriptor::NORMAL;
  parsing_result->descriptor.declaration_pos = peek_position();
  parsing_result->descriptor.initialization_pos = peek_position();
  parsing_result->descriptor.mode = VAR;

  Block* init_block = nullptr;
  if (var_context != kForStatement) {
    init_block = factory()->NewBlock(
        NULL, 1, true, parsing_result->descriptor.declaration_pos);
  }

  if (peek() == Token::VAR) {
    Consume(Token::VAR);
  } else if (peek() == Token::CONST) {
    Consume(Token::CONST);
    DCHECK(var_context != kStatement);
    parsing_result->descriptor.mode = CONST;
  } else if (peek() == Token::LET) {
    Consume(Token::LET);
    DCHECK(var_context != kStatement);
    parsing_result->descriptor.mode = LET;
  } else {
    UNREACHABLE();  // by current callers
  }

  parsing_result->descriptor.scope = scope();
  parsing_result->descriptor.hoist_scope = nullptr;


  bool first_declaration = true;
  int bindings_start = peek_position();
  do {
    FuncNameInferrer::State fni_state(fni_);

    // Parse name.
    if (!first_declaration) Consume(Token::COMMA);

    Expression* pattern;
    int decl_pos = peek_position();
    {
      ExpressionClassifier pattern_classifier(this);
      pattern = ParsePrimaryExpression(&pattern_classifier, CHECK_OK);
      ValidateBindingPattern(&pattern_classifier, CHECK_OK);
      if (IsLexicalVariableMode(parsing_result->descriptor.mode)) {
        ValidateLetPattern(&pattern_classifier, CHECK_OK);
      }
    }

    Scanner::Location variable_loc = scanner()->location();
    const AstRawString* single_name =
        pattern->IsVariableProxy() ? pattern->AsVariableProxy()->raw_name()
                                   : nullptr;
    if (single_name != nullptr) {
      if (fni_ != NULL) fni_->PushVariableName(single_name);
    }

    Expression* value = NULL;
    int initializer_position = kNoSourcePosition;
    if (Check(Token::ASSIGN)) {
      ExpressionClassifier classifier(this);
      value = ParseAssignmentExpression(var_context != kForStatement,
                                        &classifier, CHECK_OK);
      RewriteNonPattern(&classifier, CHECK_OK);
      variable_loc.end_pos = scanner()->location().end_pos;

      if (!parsing_result->first_initializer_loc.IsValid()) {
        parsing_result->first_initializer_loc = variable_loc;
      }

      // Don't infer if it is "a = function(){...}();"-like expression.
      if (single_name) {
        if (fni_ != NULL && value->AsCall() == NULL &&
            value->AsCallNew() == NULL) {
          fni_->Infer();
        } else {
          fni_->RemoveLastFunction();
        }
      }

      SetFunctionNameFromIdentifierRef(value, pattern);

      // End position of the initializer is after the assignment expression.
      initializer_position = scanner()->location().end_pos;
    } else {
      // Initializers may be either required or implied unless this is a
      // for-in/of iteration variable.
      if (var_context != kForStatement || !PeekInOrOf()) {
        // ES6 'const' and binding patterns require initializers.
        if (parsing_result->descriptor.mode == CONST ||
            !pattern->IsVariableProxy()) {
          ReportMessageAt(
              Scanner::Location(decl_pos, scanner()->location().end_pos),
              MessageTemplate::kDeclarationMissingInitializer,
              !pattern->IsVariableProxy() ? "destructuring" : "const");
          *ok = false;
          return nullptr;
        }

        // 'let x' initializes 'x' to undefined.
        if (parsing_result->descriptor.mode == LET) {
          value = GetLiteralUndefined(position());
        }
      }

      // End position of the initializer is after the variable.
      initializer_position = position();
    }

    DeclarationParsingResult::Declaration decl(pattern, initializer_position,
                                               value);
    if (var_context == kForStatement) {
      // Save the declaration for further handling in ParseForStatement.
      parsing_result->declarations.Add(decl);
    } else {
      // Immediately declare the variable otherwise. This avoids O(N^2)
      // behavior (where N is the number of variables in a single
      // declaration) in the PatternRewriter having to do with removing
      // and adding VariableProxies to the Scope (see bug 4699).
      DCHECK_NOT_NULL(init_block);
      PatternRewriter::DeclareAndInitializeVariables(
          init_block, &parsing_result->descriptor, &decl, names, CHECK_OK);
    }
    first_declaration = false;
  } while (peek() == Token::COMMA);

  parsing_result->bindings_loc =
      Scanner::Location(bindings_start, scanner()->location().end_pos);

  DCHECK(*ok);
  return init_block;
}


static bool ContainsLabel(ZoneList<const AstRawString*>* labels,
                          const AstRawString* label) {
  DCHECK(label != NULL);
  if (labels != NULL) {
    for (int i = labels->length(); i-- > 0; ) {
      if (labels->at(i) == label) {
        return true;
      }
    }
  }
  return false;
}

Statement* Parser::ParseFunctionDeclaration(bool* ok) {
  Consume(Token::FUNCTION);
  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlags::kIsNormal;
  if (Check(Token::MUL)) {
    flags |= ParseFunctionFlags::kIsGenerator;
    if (allow_harmony_restrictive_declarations()) {
      ReportMessageAt(scanner()->location(),
                      MessageTemplate::kGeneratorInLegacyContext);
      *ok = false;
      return nullptr;
    }
  }

  return ParseHoistableDeclaration(pos, flags, nullptr, false, CHECK_OK);
}

Statement* Parser::ParseExpressionOrLabelledStatement(
    ZoneList<const AstRawString*>* labels,
    AllowLabelledFunctionStatement allow_function, bool* ok) {
  // ExpressionStatement | LabelledStatement ::
  //   Expression ';'
  //   Identifier ':' Statement
  //
  // ExpressionStatement[Yield] :
  //   [lookahead ∉ {{, function, class, let [}] Expression[In, ?Yield] ;

  int pos = peek_position();

  switch (peek()) {
    case Token::FUNCTION:
    case Token::LBRACE:
      UNREACHABLE();  // Always handled by the callers.
    case Token::CLASS:
      ReportUnexpectedToken(Next());
      *ok = false;
      return nullptr;
    default:
      break;
  }

  bool starts_with_idenfifier = peek_any_identifier();
  Expression* expr = ParseExpression(true, CHECK_OK);
  if (peek() == Token::COLON && starts_with_idenfifier && expr != NULL &&
      expr->AsVariableProxy() != NULL &&
      !expr->AsVariableProxy()->is_this()) {
    // Expression is a single identifier, and not, e.g., a parenthesized
    // identifier.
    VariableProxy* var = expr->AsVariableProxy();
    const AstRawString* label = var->raw_name();
    // TODO(1240780): We don't check for redeclaration of labels
    // during preparsing since keeping track of the set of active
    // labels requires nontrivial changes to the way scopes are
    // structured.  However, these are probably changes we want to
    // make later anyway so we should go back and fix this then.
    if (ContainsLabel(labels, label) || TargetStackContainsLabel(label)) {
      ReportMessage(MessageTemplate::kLabelRedeclaration, label);
      *ok = false;
      return NULL;
    }
    if (labels == NULL) {
      labels = new(zone()) ZoneList<const AstRawString*>(4, zone());
    }
    labels->Add(label, zone());
    // Remove the "ghost" variable that turned out to be a label
    // from the top scope. This way, we don't try to resolve it
    // during the scope processing.
    scope()->RemoveUnresolved(var);
    Expect(Token::COLON, CHECK_OK);
    // ES#sec-labelled-function-declarations Labelled Function Declarations
    if (peek() == Token::FUNCTION && is_sloppy(language_mode())) {
      if (allow_function == kAllowLabelledFunctionStatement) {
        return ParseFunctionDeclaration(ok);
      } else {
        return ParseScopedStatement(labels, true, ok);
      }
    }
    return ParseStatement(labels, kDisallowLabelledFunctionStatement, ok);
  }

  // If we have an extension, we allow a native function declaration.
  // A native function declaration starts with "native function" with
  // no line-terminator between the two words.
  if (extension_ != NULL && peek() == Token::FUNCTION &&
      !scanner()->HasAnyLineTerminatorBeforeNext() && expr != NULL &&
      expr->AsVariableProxy() != NULL &&
      expr->AsVariableProxy()->raw_name() ==
          ast_value_factory()->native_string() &&
      !scanner()->literal_contains_escapes()) {
    return ParseNativeDeclaration(ok);
  }

  // Parsed expression statement, followed by semicolon.
  ExpectSemicolon(CHECK_OK);
  return factory()->NewExpressionStatement(expr, pos);
}


IfStatement* Parser::ParseIfStatement(ZoneList<const AstRawString*>* labels,
                                      bool* ok) {
  // IfStatement ::
  //   'if' '(' Expression ')' Statement ('else' Statement)?

  int pos = peek_position();
  Expect(Token::IF, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* condition = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  Statement* then_statement = ParseScopedStatement(labels, false, CHECK_OK);
  Statement* else_statement = NULL;
  if (peek() == Token::ELSE) {
    Next();
    else_statement = ParseScopedStatement(labels, false, CHECK_OK);
  } else {
    else_statement = factory()->NewEmptyStatement(kNoSourcePosition);
  }
  return factory()->NewIfStatement(
      condition, then_statement, else_statement, pos);
}


Statement* Parser::ParseContinueStatement(bool* ok) {
  // ContinueStatement ::
  //   'continue' Identifier? ';'

  int pos = peek_position();
  Expect(Token::CONTINUE, CHECK_OK);
  const AstRawString* label = NULL;
  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      tok != Token::SEMICOLON && tok != Token::RBRACE && tok != Token::EOS) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    label = ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  }
  IterationStatement* target = LookupContinueTarget(label, CHECK_OK);
  if (target == NULL) {
    // Illegal continue statement.
    MessageTemplate::Template message = MessageTemplate::kIllegalContinue;
    if (label != NULL) {
      message = MessageTemplate::kUnknownLabel;
    }
    ReportMessage(message, label);
    *ok = false;
    return NULL;
  }
  ExpectSemicolon(CHECK_OK);
  return factory()->NewContinueStatement(target, pos);
}


Statement* Parser::ParseBreakStatement(ZoneList<const AstRawString*>* labels,
                                       bool* ok) {
  // BreakStatement ::
  //   'break' Identifier? ';'

  int pos = peek_position();
  Expect(Token::BREAK, CHECK_OK);
  const AstRawString* label = NULL;
  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      tok != Token::SEMICOLON && tok != Token::RBRACE && tok != Token::EOS) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    label = ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  }
  // Parse labeled break statements that target themselves into
  // empty statements, e.g. 'l1: l2: l3: break l2;'
  if (label != NULL && ContainsLabel(labels, label)) {
    ExpectSemicolon(CHECK_OK);
    return factory()->NewEmptyStatement(pos);
  }
  BreakableStatement* target = NULL;
  target = LookupBreakTarget(label, CHECK_OK);
  if (target == NULL) {
    // Illegal break statement.
    MessageTemplate::Template message = MessageTemplate::kIllegalBreak;
    if (label != NULL) {
      message = MessageTemplate::kUnknownLabel;
    }
    ReportMessage(message, label);
    *ok = false;
    return NULL;
  }
  ExpectSemicolon(CHECK_OK);
  return factory()->NewBreakStatement(target, pos);
}


Statement* Parser::ParseReturnStatement(bool* ok) {
  // ReturnStatement ::
  //   'return' Expression? ';'

  // Consume the return token. It is necessary to do that before
  // reporting any errors on it, because of the way errors are
  // reported (underlining).
  Expect(Token::RETURN, CHECK_OK);
  Scanner::Location loc = scanner()->location();

  Token::Value tok = peek();
  Statement* result;
  Expression* return_value;
  if (scanner()->HasAnyLineTerminatorBeforeNext() ||
      tok == Token::SEMICOLON ||
      tok == Token::RBRACE ||
      tok == Token::EOS) {
    if (IsSubclassConstructor(function_state_->kind())) {
      return_value = ThisExpression(loc.beg_pos);
    } else {
      return_value = GetLiteralUndefined(position());
    }
  } else {
    int pos = peek_position();

    if (IsSubclassConstructor(function_state_->kind())) {
      // Because of the return code rewriting that happens in case of a subclass
      // constructor we don't want to accept tail calls, therefore we don't set
      // ReturnExprScope to kInsideValidReturnStatement here.
      return_value = ParseExpression(true, CHECK_OK);

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
      return_value = factory()->NewConditional(
          is_undefined, ThisExpression(pos), is_object_conditional, pos);
    } else {
      ReturnExprScope maybe_allow_tail_calls(
          function_state_, ReturnExprContext::kInsideValidReturnStatement);
      return_value = ParseExpression(true, CHECK_OK);

      if (allow_tailcalls() && !is_sloppy(language_mode())) {
        // ES6 14.6.1 Static Semantics: IsInTailPosition
        function_state_->AddImplicitTailCallExpression(return_value);
      }
    }
  }
  ExpectSemicolon(CHECK_OK);

  if (is_generator()) {
    return_value = BuildIteratorResult(return_value, true);
  } else if (is_async_function()) {
    return_value = BuildPromiseResolve(return_value, return_value->position());
  }

  result = factory()->NewReturnStatement(return_value, loc.beg_pos);

  DeclarationScope* decl_scope = GetDeclarationScope();
  if (decl_scope->is_script_scope() || decl_scope->is_eval_scope()) {
    ReportMessageAt(loc, MessageTemplate::kIllegalReturn);
    *ok = false;
    return NULL;
  }
  return result;
}


Statement* Parser::ParseWithStatement(ZoneList<const AstRawString*>* labels,
                                      bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement

  Expect(Token::WITH, CHECK_OK);
  int pos = position();

  if (is_strict(language_mode())) {
    ReportMessage(MessageTemplate::kStrictWith);
    *ok = false;
    return NULL;
  }

  Expect(Token::LPAREN, CHECK_OK);
  Expression* expr = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  Scope* with_scope = NewScope(WITH_SCOPE);
  Statement* body;
  {
    BlockState block_state(&scope_state_, with_scope);
    with_scope->set_start_position(scanner()->peek_location().beg_pos);
    body = ParseScopedStatement(labels, true, CHECK_OK);
    with_scope->set_end_position(scanner()->location().end_pos);
  }
  return factory()->NewWithStatement(with_scope, expr, body, pos);
}


CaseClause* Parser::ParseCaseClause(bool* default_seen_ptr, bool* ok) {
  // CaseClause ::
  //   'case' Expression ':' StatementList
  //   'default' ':' StatementList

  Expression* label = NULL;  // NULL expression indicates default case
  if (peek() == Token::CASE) {
    Expect(Token::CASE, CHECK_OK);
    label = ParseExpression(true, CHECK_OK);
  } else {
    Expect(Token::DEFAULT, CHECK_OK);
    if (*default_seen_ptr) {
      ReportMessage(MessageTemplate::kMultipleDefaultsInSwitch);
      *ok = false;
      return NULL;
    }
    *default_seen_ptr = true;
  }
  Expect(Token::COLON, CHECK_OK);
  int pos = position();
  ZoneList<Statement*>* statements =
      new(zone()) ZoneList<Statement*>(5, zone());
  Statement* stat = NULL;
  while (peek() != Token::CASE &&
         peek() != Token::DEFAULT &&
         peek() != Token::RBRACE) {
    stat = ParseStatementListItem(CHECK_OK);
    statements->Add(stat, zone());
  }
  return factory()->NewCaseClause(label, statements, pos);
}


Statement* Parser::ParseSwitchStatement(ZoneList<const AstRawString*>* labels,
                                        bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'
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
  int switch_pos = peek_position();

  Expect(Token::SWITCH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* tag = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

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

  Block* cases_block = factory()->NewBlock(NULL, 1, false, kNoSourcePosition);

  SwitchStatement* switch_statement =
      factory()->NewSwitchStatement(labels, switch_pos);

  {
    BlockState cases_block_state(&scope_state_);
    cases_block_state.set_start_position(scanner()->location().beg_pos);
    cases_block_state.SetNonlinear();
    Target target(&this->target_stack_, switch_statement);

    Expression* tag_read = factory()->NewVariableProxy(tag_variable);

    bool default_seen = false;
    ZoneList<CaseClause*>* cases =
        new (zone()) ZoneList<CaseClause*>(4, zone());
    Expect(Token::LBRACE, CHECK_OK);
    while (peek() != Token::RBRACE) {
      CaseClause* clause = ParseCaseClause(&default_seen, CHECK_OK);
      cases->Add(clause, zone());
    }
    switch_statement->Initialize(tag_read, cases);
    cases_block->statements()->Add(switch_statement, zone());
    Expect(Token::RBRACE, CHECK_OK);

    cases_block_state.set_end_position(scanner()->location().end_pos);
    cases_block->set_scope(cases_block_state.FinalizedBlockScope());
  }

  switch_block->statements()->Add(cases_block, zone());

  return switch_block;
}


Statement* Parser::ParseThrowStatement(bool* ok) {
  // ThrowStatement ::
  //   'throw' Expression ';'

  Expect(Token::THROW, CHECK_OK);
  int pos = position();
  if (scanner()->HasAnyLineTerminatorBeforeNext()) {
    ReportMessage(MessageTemplate::kNewlineAfterThrow);
    *ok = false;
    return NULL;
  }
  Expression* exception = ParseExpression(true, CHECK_OK);
  ExpectSemicolon(CHECK_OK);

  return factory()->NewExpressionStatement(
      factory()->NewThrow(exception, pos), pos);
}


TryStatement* Parser::ParseTryStatement(bool* ok) {
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
  int pos = position();

  Block* try_block;
  {
    ReturnExprScope no_tail_calls(function_state_,
                                  ReturnExprContext::kInsideTryBlock);
    try_block = ParseBlock(NULL, CHECK_OK);
  }

  Token::Value tok = peek();

  bool catch_for_promise_reject = false;
  if (allow_natives() && tok == Token::MOD) {
    Consume(Token::MOD);
    catch_for_promise_reject = true;
    tok = peek();
  }

  if (tok != Token::CATCH && tok != Token::FINALLY) {
    ReportMessage(MessageTemplate::kNoCatchOrFinally);
    *ok = false;
    return NULL;
  }

  Scope* catch_scope = NULL;
  Variable* catch_variable = NULL;
  Block* catch_block = NULL;
  TailCallExpressionList tail_call_expressions_in_catch_block(zone());
  if (tok == Token::CATCH) {
    Consume(Token::CATCH);

    Expect(Token::LPAREN, CHECK_OK);
    catch_scope = NewScope(CATCH_SCOPE);
    catch_scope->set_start_position(scanner()->location().beg_pos);

    {
      CollectExpressionsInTailPositionToListScope
          collect_tail_call_expressions_scope(
              function_state_, &tail_call_expressions_in_catch_block);
      BlockState block_state(&scope_state_, catch_scope);

      catch_block = factory()->NewBlock(nullptr, 16, false, kNoSourcePosition);

      // Create a block scope to hold any lexical declarations created
      // as part of destructuring the catch parameter.
      {
        BlockState block_state(&scope_state_);
        block_state.set_start_position(scanner()->location().beg_pos);
        Target target(&this->target_stack_, catch_block);

        const AstRawString* name = ast_value_factory()->dot_catch_string();
        Expression* pattern = nullptr;
        if (peek_any_identifier()) {
          name = ParseIdentifier(kDontAllowRestrictedIdentifiers, CHECK_OK);
        } else {
          ExpressionClassifier pattern_classifier(this);
          pattern = ParsePrimaryExpression(&pattern_classifier, CHECK_OK);
          ValidateBindingPattern(&pattern_classifier, CHECK_OK);
        }
        catch_variable = catch_scope->DeclareLocal(
            name, VAR, kCreatedInitialized, Variable::NORMAL);

        Expect(Token::RPAREN, CHECK_OK);

        ZoneList<const AstRawString*> bound_names(1, zone());
        if (pattern != nullptr) {
          DeclarationDescriptor descriptor;
          descriptor.declaration_kind = DeclarationDescriptor::NORMAL;
          descriptor.parser = this;
          descriptor.scope = scope();
          descriptor.hoist_scope = nullptr;
          descriptor.mode = LET;
          descriptor.declaration_pos = pattern->position();
          descriptor.initialization_pos = pattern->position();

          // Initializer position for variables declared by the pattern.
          const int initializer_position = position();

          DeclarationParsingResult::Declaration decl(
              pattern, initializer_position,
              factory()->NewVariableProxy(catch_variable));

          Block* init_block =
              factory()->NewBlock(nullptr, 8, true, kNoSourcePosition);
          PatternRewriter::DeclareAndInitializeVariables(
              init_block, &descriptor, &decl, &bound_names, CHECK_OK);
          catch_block->statements()->Add(init_block, zone());
        } else {
          bound_names.Add(name, zone());
        }

        Block* inner_block = ParseBlock(nullptr, CHECK_OK);
        catch_block->statements()->Add(inner_block, zone());

        // Check for `catch(e) { let e; }` and similar errors.
        Scope* inner_block_scope = inner_block->scope();
        if (inner_block_scope != nullptr) {
          Declaration* decl =
              inner_block_scope->CheckLexDeclarationsConflictingWith(
                  bound_names);
          if (decl != nullptr) {
            const AstRawString* name = decl->proxy()->raw_name();
            int position = decl->proxy()->position();
            Scanner::Location location =
                position == kNoSourcePosition
                    ? Scanner::Location::invalid()
                    : Scanner::Location(position, position + 1);
            ReportMessageAt(location, MessageTemplate::kVarRedeclaration, name);
            *ok = false;
            return nullptr;
          }
        }
        block_state.set_end_position(scanner()->location().end_pos);
        catch_block->set_scope(block_state.FinalizedBlockScope());
      }
    }

    catch_scope->set_end_position(scanner()->location().end_pos);
    tok = peek();
  }

  Block* finally_block = NULL;
  DCHECK(tok == Token::FINALLY || catch_block != NULL);
  if (tok == Token::FINALLY) {
    Consume(Token::FINALLY);
    finally_block = ParseBlock(NULL, CHECK_OK);
  }

  // Simplify the AST nodes by converting:
  //   'try B0 catch B1 finally B2'
  // to:
  //   'try { try B0 catch B1 } finally B2'

  if (catch_block != NULL && finally_block != NULL) {
    // If we have both, create an inner try/catch.
    DCHECK(catch_scope != NULL && catch_variable != NULL);
    TryCatchStatement* statement;
    if (catch_for_promise_reject) {
      statement = factory()->NewTryCatchStatementForPromiseReject(
          try_block, catch_scope, catch_variable, catch_block,
          kNoSourcePosition);
    } else {
      statement = factory()->NewTryCatchStatement(try_block, catch_scope,
                                                  catch_variable, catch_block,
                                                  kNoSourcePosition);
    }

    try_block = factory()->NewBlock(NULL, 1, false, kNoSourcePosition);
    try_block->statements()->Add(statement, zone());
    catch_block = NULL;  // Clear to indicate it's been handled.
  }

  TryStatement* result = NULL;
  if (catch_block != NULL) {
    // For a try-catch construct append return expressions from the catch block
    // to the list of return expressions.
    function_state_->tail_call_expressions().Append(
        tail_call_expressions_in_catch_block);

    DCHECK(finally_block == NULL);
    DCHECK(catch_scope != NULL && catch_variable != NULL);
    result = factory()->NewTryCatchStatement(try_block, catch_scope,
                                             catch_variable, catch_block, pos);
  } else {
    if (FLAG_harmony_explicit_tailcalls &&
        tail_call_expressions_in_catch_block.has_explicit_tail_calls()) {
      // TODO(ishell): update chapter number.
      // ES8 XX.YY.ZZ
      ReportMessageAt(tail_call_expressions_in_catch_block.location(),
                      MessageTemplate::kUnexpectedTailCallInCatchBlock);
      *ok = false;
      return NULL;
    }
    DCHECK(finally_block != NULL);
    result = factory()->NewTryFinallyStatement(try_block, finally_block, pos);
  }

  return result;
}


DoWhileStatement* Parser::ParseDoWhileStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'

  DoWhileStatement* loop =
      factory()->NewDoWhileStatement(labels, peek_position());
  Target target(&this->target_stack_, loop);

  Expect(Token::DO, CHECK_OK);
  Statement* body = ParseScopedStatement(NULL, true, CHECK_OK);
  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);

  Expression* cond = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  // Allow do-statements to be terminated with and without
  // semi-colons. This allows code such as 'do;while(0)return' to
  // parse, which would not be the case if we had used the
  // ExpectSemicolon() functionality here.
  if (peek() == Token::SEMICOLON) Consume(Token::SEMICOLON);

  if (loop != NULL) loop->Initialize(cond, body);
  return loop;
}


WhileStatement* Parser::ParseWhileStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // WhileStatement ::
  //   'while' '(' Expression ')' Statement

  WhileStatement* loop = factory()->NewWhileStatement(labels, peek_position());
  Target target(&this->target_stack_, loop);

  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* cond = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  Statement* body = ParseScopedStatement(NULL, true, CHECK_OK);

  if (loop != NULL) loop->Initialize(cond, body);
  return loop;
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
    stmt->AsForInStatement()->Initialize(each, subject, body);
  }
  return stmt;
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

  Variable* iterator = NewTemporary(ast_value_factory()->dot_iterator_string());
  Variable* result = NewTemporary(ast_value_factory()->dot_result_string());
  Variable* completion = NewTemporary(avfactory->empty_string());

  // iterator = iterable[Symbol.iterator]()
  Expression* assign_iterator;
  {
    assign_iterator = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(iterator),
        GetIterator(iterable, iterable->position()), iterable->position());
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
    Scope* inner_scope, VariableMode mode, ZoneList<const AstRawString*>* names,
    ForStatement* loop, Statement* init, Expression* cond, Statement* next,
    Statement* body, bool* ok) {
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

  DCHECK(names->length() > 0);
  ZoneList<Variable*> temps(names->length(), zone());

  Block* outer_block =
      factory()->NewBlock(NULL, names->length() + 4, false, kNoSourcePosition);

  // Add statement: let/const x = i.
  outer_block->statements()->Add(init, zone());

  const AstRawString* temp_name = ast_value_factory()->dot_for_string();

  // For each lexical variable x:
  //   make statement: temp_x = x.
  for (int i = 0; i < names->length(); i++) {
    VariableProxy* proxy = NewUnresolved(names->at(i));
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

    Block* ignore_completion_block =
        factory()->NewBlock(NULL, names->length() + 3, true, kNoSourcePosition);
    ZoneList<Variable*> inner_vars(names->length(), zone());
    // For each let variable x:
    //    make statement: let/const x = temp_x.
    for (int i = 0; i < names->length(); i++) {
      Declaration* decl =
          DeclareVariable(names->at(i), mode, kNoSourcePosition, CHECK_OK);
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
      for (int i = 0; i < names->length(); i++) {
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

Statement* Parser::ParseScopedStatement(ZoneList<const AstRawString*>* labels,
                                        bool legacy, bool* ok) {
  if (is_strict(language_mode()) || peek() != Token::FUNCTION ||
      (legacy && allow_harmony_restrictive_declarations())) {
    return ParseSubStatement(labels, kDisallowLabelledFunctionStatement, ok);
  } else {
    if (legacy) {
      ++use_counts_[v8::Isolate::kLegacyFunctionDeclaration];
    }
    // Make a block around the statement for a lexical binding
    // is introduced by a FunctionDeclaration.
    BlockState block_state(&scope_state_);
    block_state.set_start_position(scanner()->location().beg_pos);
    Block* block = factory()->NewBlock(NULL, 1, false, kNoSourcePosition);
    Statement* body = ParseFunctionDeclaration(CHECK_OK);
    block->statements()->Add(body, zone());
    block_state.set_end_position(scanner()->location().end_pos);
    block->set_scope(block_state.FinalizedBlockScope());
    return block;
  }
}

Statement* Parser::ParseForStatement(ZoneList<const AstRawString*>* labels,
                                     bool* ok) {
  int stmt_pos = peek_position();
  Statement* init = NULL;
  ZoneList<const AstRawString*> bound_names(1, zone());
  bool bound_names_are_lexical = false;

  // Create an in-between scope for let-bound iteration variables.
  BlockState for_state(&scope_state_);
  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  for_state.set_start_position(scanner()->location().beg_pos);
  for_state.set_is_hidden();
  DeclarationParsingResult parsing_result;
  if (peek() != Token::SEMICOLON) {
    if (peek() == Token::VAR || peek() == Token::CONST ||
        (peek() == Token::LET && IsNextLetKeyword())) {
      ParseVariableDeclarations(kForStatement, &parsing_result, nullptr,
                                CHECK_OK);

      ForEachStatement::VisitMode mode = ForEachStatement::ENUMERATE;
      int each_beg_pos = scanner()->location().beg_pos;
      int each_end_pos = scanner()->location().end_pos;

      if (CheckInOrOf(&mode, ok)) {
        if (!*ok) return nullptr;
        if (parsing_result.declarations.length() != 1) {
          ReportMessageAt(parsing_result.bindings_loc,
                          MessageTemplate::kForInOfLoopMultiBindings,
                          ForEachStatement::VisitModeString(mode));
          *ok = false;
          return nullptr;
        }
        DeclarationParsingResult::Declaration& decl =
            parsing_result.declarations[0];
        if (parsing_result.first_initializer_loc.IsValid() &&
            (is_strict(language_mode()) || mode == ForEachStatement::ITERATE ||
             IsLexicalVariableMode(parsing_result.descriptor.mode) ||
             !decl.pattern->IsVariableProxy() || allow_harmony_for_in())) {
          // Only increment the use count if we would have let this through
          // without the flag.
          if (allow_harmony_for_in()) {
            ++use_counts_[v8::Isolate::kForInInitializer];
          }
          ReportMessageAt(parsing_result.first_initializer_loc,
                          MessageTemplate::kForInOfLoopInitializer,
                          ForEachStatement::VisitModeString(mode));
          *ok = false;
          return nullptr;
        }

        Block* init_block = nullptr;
        bound_names_are_lexical =
            IsLexicalVariableMode(parsing_result.descriptor.mode);

        // special case for legacy for (var ... = ... in ...)
        if (!bound_names_are_lexical && decl.pattern->IsVariableProxy() &&
            decl.initializer != nullptr) {
          DCHECK(!allow_harmony_for_in());
          ++use_counts_[v8::Isolate::kForInInitializer];
          const AstRawString* name =
              decl.pattern->AsVariableProxy()->raw_name();
          VariableProxy* single_var = NewUnresolved(name);
          init_block = factory()->NewBlock(
              nullptr, 2, true, parsing_result.descriptor.declaration_pos);
          init_block->statements()->Add(
              factory()->NewExpressionStatement(
                  factory()->NewAssignment(Token::ASSIGN, single_var,
                                           decl.initializer, kNoSourcePosition),
                  kNoSourcePosition),
              zone());
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

        Variable* temp = NewTemporary(ast_value_factory()->dot_for_string());
        ForEachStatement* loop =
            factory()->NewForEachStatement(mode, labels, stmt_pos);
        Target target(&this->target_stack_, loop);

        int each_keyword_position = scanner()->location().beg_pos;

        Expression* enumerable;
        if (mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          enumerable = ParseAssignmentExpression(true, &classifier, CHECK_OK);
          RewriteNonPattern(&classifier, CHECK_OK);
        } else {
          enumerable = ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);


        Block* body_block =
            factory()->NewBlock(NULL, 3, false, kNoSourcePosition);

        Statement* final_loop;
        {
          ReturnExprScope no_tail_calls(function_state_,
                                        ReturnExprContext::kInsideForInOfBody);
          BlockState block_state(&scope_state_);
          block_state.set_start_position(scanner()->location().beg_pos);

          Statement* body = ParseScopedStatement(NULL, true, CHECK_OK);

          auto each_initialization_block =
              factory()->NewBlock(nullptr, 1, true, kNoSourcePosition);
          {
            auto descriptor = parsing_result.descriptor;
            descriptor.declaration_pos = kNoSourcePosition;
            descriptor.initialization_pos = kNoSourcePosition;
            decl.initializer = factory()->NewVariableProxy(temp);

            bool is_for_var_of =
                mode == ForEachStatement::ITERATE &&
                parsing_result.descriptor.mode == VariableMode::VAR;

            PatternRewriter::DeclareAndInitializeVariables(
                each_initialization_block, &descriptor, &decl,
                bound_names_are_lexical || is_for_var_of ? &bound_names
                                                         : nullptr,
                CHECK_OK);

            // Annex B.3.5 prohibits the form
            // `try {} catch(e) { for (var e of {}); }`
            // So if we are parsing a statement like `for (var ... of ...)`
            // we need to walk up the scope chain and look for catch scopes
            // which have a simple binding, then compare their binding against
            // all of the names declared in the init of the for-of we're
            // parsing.
            if (is_for_var_of) {
              Scope* catch_scope = scope();
              while (catch_scope != nullptr &&
                     !catch_scope->is_declaration_scope()) {
                if (catch_scope->is_catch_scope()) {
                  auto name = catch_scope->catch_variable_name();
                  if (name !=
                      ast_value_factory()
                          ->dot_catch_string()) {  // i.e. is a simple binding
                    if (bound_names.Contains(name)) {
                      ReportMessageAt(parsing_result.bindings_loc,
                                      MessageTemplate::kVarRedeclaration, name);
                      *ok = false;
                      return nullptr;
                    }
                  }
                }
                catch_scope = catch_scope->outer_scope();
              }
            }
          }

          body_block->statements()->Add(each_initialization_block, zone());
          body_block->statements()->Add(body, zone());
          VariableProxy* temp_proxy =
              factory()->NewVariableProxy(temp, each_beg_pos, each_end_pos);
          final_loop = InitializeForEachStatement(
              loop, temp_proxy, enumerable, body_block, each_keyword_position);
          block_state.set_end_position(scanner()->location().end_pos);
          body_block->set_scope(block_state.FinalizedBlockScope());
        }

        // Create a TDZ for any lexically-bound names.
        if (bound_names_are_lexical) {
          DCHECK_NULL(init_block);

          init_block =
              factory()->NewBlock(nullptr, 1, false, kNoSourcePosition);

          for (int i = 0; i < bound_names.length(); ++i) {
            // TODO(adamk): This needs to be some sort of special
            // INTERNAL variable that's invisible to the debugger
            // but visible to everything else.
            Declaration* tdz_decl = DeclareVariable(
                bound_names[i], LET, kNoSourcePosition, CHECK_OK);
            tdz_decl->proxy()->var()->set_initializer_position(position());
          }
        }

        for_state.set_end_position(scanner()->location().end_pos);
        Scope* for_scope = for_state.FinalizedBlockScope();
        // Parsed for-in loop w/ variable declarations.
        if (init_block != nullptr) {
          init_block->statements()->Add(final_loop, zone());
          init_block->set_scope(for_scope);
          return init_block;
        } else {
          DCHECK_NULL(for_scope);
          return final_loop;
        }
      } else {
        bound_names_are_lexical =
            IsLexicalVariableMode(parsing_result.descriptor.mode);
        init = parsing_result.BuildInitializationBlock(
            bound_names_are_lexical ? &bound_names : nullptr, CHECK_OK);
      }
    } else {
      int lhs_beg_pos = peek_position();
      ExpressionClassifier classifier(this);
      Expression* expression = ParseExpression(false, &classifier, CHECK_OK);
      int lhs_end_pos = scanner()->location().end_pos;
      ForEachStatement::VisitMode mode = ForEachStatement::ENUMERATE;

      bool is_for_each = CheckInOrOf(&mode, CHECK_OK);
      bool is_destructuring = is_for_each && (expression->IsArrayLiteral() ||
                                              expression->IsObjectLiteral());

      if (is_destructuring) {
        ValidateAssignmentPattern(&classifier, CHECK_OK);
      } else {
        RewriteNonPattern(&classifier, CHECK_OK);
      }

      if (is_for_each) {
        if (!is_destructuring) {
          expression = CheckAndRewriteReferenceExpression(
              expression, lhs_beg_pos, lhs_end_pos,
              MessageTemplate::kInvalidLhsInFor, kSyntaxError, CHECK_OK);
        }

        ForEachStatement* loop =
            factory()->NewForEachStatement(mode, labels, stmt_pos);
        Target target(&this->target_stack_, loop);

        int each_keyword_position = scanner()->location().beg_pos;

        Expression* enumerable;
        if (mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          enumerable = ParseAssignmentExpression(true, &classifier, CHECK_OK);
          RewriteNonPattern(&classifier, CHECK_OK);
        } else {
          enumerable = ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);

        // For legacy compat reasons, give for loops similar treatment to
        // if statements in allowing a function declaration for a body
        Statement* body = ParseScopedStatement(NULL, true, CHECK_OK);
        Statement* final_loop = InitializeForEachStatement(
            loop, expression, enumerable, body, each_keyword_position);

        DCHECK_NULL(for_state.FinalizedBlockScope());
        return final_loop;

      } else {
        init = factory()->NewExpressionStatement(expression, lhs_beg_pos);
      }
    }
  }

  // Standard 'for' loop
  ForStatement* loop = factory()->NewForStatement(labels, stmt_pos);
  Target target(&this->target_stack_, loop);

  // Parsed initializer at this point.
  Expect(Token::SEMICOLON, CHECK_OK);

  Expression* cond = NULL;
  Statement* next = NULL;
  Statement* body = NULL;

  // If there are let bindings, then condition and the next statement of the
  // for loop must be parsed in a new scope.
  Scope* inner_scope = scope();
  // TODO(verwaest): Allocate this through a ScopeState as well.
  if (bound_names_are_lexical && bound_names.length() > 0) {
    inner_scope = NewScopeWithParent(inner_scope, BLOCK_SCOPE);
    inner_scope->set_start_position(scanner()->location().beg_pos);
  }
  {
    BlockState block_state(&scope_state_, inner_scope);

    if (peek() != Token::SEMICOLON) {
      cond = ParseExpression(true, CHECK_OK);
    }
    Expect(Token::SEMICOLON, CHECK_OK);

    if (peek() != Token::RPAREN) {
      Expression* exp = ParseExpression(true, CHECK_OK);
      next = factory()->NewExpressionStatement(exp, exp->position());
    }
    Expect(Token::RPAREN, CHECK_OK);

    body = ParseScopedStatement(NULL, true, CHECK_OK);
  }

  Statement* result = NULL;
  if (bound_names_are_lexical && bound_names.length() > 0) {
    result = DesugarLexicalBindingsInForStatement(
        inner_scope, parsing_result.descriptor.mode, &bound_names, loop, init,
        cond, next, body, CHECK_OK);
    for_state.set_end_position(scanner()->location().end_pos);
  } else {
    for_state.set_end_position(scanner()->location().end_pos);
    Scope* for_scope = for_state.FinalizedBlockScope();
    if (for_scope) {
      // Rewrite a for statement of the form
      //   for (const x = i; c; n) b
      //
      // into
      //
      //   {
      //     const x = i;
      //     for (; c; n) b
      //   }
      //
      // or, desugar
      //   for (; c; n) b
      // into
      //   {
      //     for (; c; n) b
      //   }
      // just in case b introduces a lexical binding some other way, e.g., if b
      // is a FunctionDeclaration.
      Block* block = factory()->NewBlock(NULL, 2, false, kNoSourcePosition);
      if (init != nullptr) {
        block->statements()->Add(init, zone());
      }
      block->statements()->Add(loop, zone());
      block->set_scope(for_scope);
      loop->Initialize(NULL, cond, next, body);
      result = block;
    } else {
      loop->Initialize(init, cond, next, body);
      result = loop;
    }
  }
  return result;
}


DebuggerStatement* Parser::ParseDebuggerStatement(bool* ok) {
  // In ECMA-262 'debugger' is defined as a reserved keyword. In some browser
  // contexts this is used as a statement which invokes the debugger as i a
  // break point is present.
  // DebuggerStatement ::
  //   'debugger' ';'

  int pos = peek_position();
  Expect(Token::DEBUGGER, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return factory()->NewDebuggerStatement(pos);
}


bool CompileTimeValue::IsCompileTimeValue(Expression* expression) {
  if (expression->IsLiteral()) return true;
  MaterializedLiteral* lit = expression->AsMaterializedLiteral();
  return lit != NULL && lit->is_simple();
}


Handle<FixedArray> CompileTimeValue::GetValue(Isolate* isolate,
                                              Expression* expression) {
  Factory* factory = isolate->factory();
  DCHECK(IsCompileTimeValue(expression));
  Handle<FixedArray> result = factory->NewFixedArray(2, TENURED);
  ObjectLiteral* object_literal = expression->AsObjectLiteral();
  if (object_literal != NULL) {
    DCHECK(object_literal->is_simple());
    if (object_literal->fast_elements()) {
      result->set(kLiteralTypeSlot, Smi::FromInt(OBJECT_LITERAL_FAST_ELEMENTS));
    } else {
      result->set(kLiteralTypeSlot, Smi::FromInt(OBJECT_LITERAL_SLOW_ELEMENTS));
    }
    result->set(kElementsSlot, *object_literal->constant_properties());
  } else {
    ArrayLiteral* array_literal = expression->AsArrayLiteral();
    DCHECK(array_literal != NULL && array_literal->is_simple());
    result->set(kLiteralTypeSlot, Smi::FromInt(ARRAY_LITERAL));
    result->set(kElementsSlot, *array_literal->constant_elements());
  }
  return result;
}


CompileTimeValue::LiteralType CompileTimeValue::GetLiteralType(
    Handle<FixedArray> value) {
  Smi* literal_type = Smi::cast(value->get(kLiteralTypeSlot));
  return static_cast<LiteralType>(literal_type->value());
}


Handle<FixedArray> CompileTimeValue::GetElements(Handle<FixedArray> value) {
  return Handle<FixedArray>(FixedArray::cast(value->get(kElementsSlot)));
}

void Parser::ParseArrowFunctionFormalParameters(
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
    ParseArrowFunctionFormalParameters(parameters, left, comma_pos,
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

void Parser::DesugarAsyncFunctionBody(const AstRawString* function_name,
                                      Scope* scope, ZoneList<Statement*>* body,
                                      ExpressionClassifier* classifier,
                                      FunctionKind kind,
                                      FunctionBodyType body_type,
                                      bool accept_IN, int pos, bool* ok) {
  // function async_function() {
  //   try {
  //     .generator_object = %CreateGeneratorObject();
  //     ... function body ...
  //   } catch (e) {
  //     return Promise.reject(e);
  //   }
  // }
  scope->ForceContextAllocation();
  Variable* temp =
      NewTemporary(ast_value_factory()->dot_generator_object_string());
  function_state_->set_generator_object_variable(temp);

  Expression* init_generator_variable = factory()->NewAssignment(
      Token::INIT, factory()->NewVariableProxy(temp),
      BuildCreateJSGeneratorObject(pos, kind), kNoSourcePosition);
  body->Add(factory()->NewExpressionStatement(init_generator_variable,
                                              kNoSourcePosition),
            zone());

  Block* try_block = factory()->NewBlock(NULL, 8, true, kNoSourcePosition);

  ZoneList<Statement*>* inner_body = try_block->statements();

  Expression* return_value = nullptr;
  if (body_type == FunctionBodyType::kNormal) {
    ParseStatementList(inner_body, Token::RBRACE, CHECK_OK_VOID);
    return_value = factory()->NewUndefinedLiteral(kNoSourcePosition);
  } else {
    return_value =
        ParseAssignmentExpression(accept_IN, classifier, CHECK_OK_VOID);
    RewriteNonPattern(classifier, CHECK_OK_VOID);
  }

  return_value = BuildPromiseResolve(return_value, return_value->position());
  inner_body->Add(
      factory()->NewReturnStatement(return_value, return_value->position()),
      zone());
  body->Add(BuildRejectPromiseOnException(try_block), zone());
  scope->set_end_position(scanner()->location().end_pos);
}

DoExpression* Parser::ParseDoExpression(bool* ok) {
  // AssignmentExpression ::
  //     do '{' StatementList '}'
  int pos = peek_position();

  Expect(Token::DO, CHECK_OK);
  Variable* result = NewTemporary(ast_value_factory()->dot_result_string());
  Block* block = ParseBlock(nullptr, CHECK_OK);
  DoExpression* expr = factory()->NewDoExpression(block, result, pos);
  if (!Rewriter::Rewrite(this, GetClosureScope(), expr, ast_value_factory())) {
    *ok = false;
    return nullptr;
  }
  return expr;
}

void Parser::ParseArrowFunctionFormalParameterList(
    ParserFormalParameters* parameters, Expression* expr,
    const Scanner::Location& params_loc, Scanner::Location* duplicate_loc,
    const Scope::Snapshot& scope_snapshot, bool* ok) {
  if (expr->IsEmptyParentheses()) return;

  ParseArrowFunctionFormalParameters(parameters, expr, params_loc.end_pos,
                                     CHECK_OK_VOID);

  scope_snapshot.Reparent(parameters->scope);

  if (parameters->Arity() > Code::kMaxArguments) {
    ReportMessageAt(params_loc, MessageTemplate::kMalformedArrowFunParamList);
    *ok = false;
    return;
  }

  Type::ExpressionClassifier classifier(this);
  if (!parameters->is_simple) {
    classifier.RecordNonSimpleParameter();
  }
  for (int i = 0; i < parameters->Arity(); ++i) {
    auto parameter = parameters->at(i);
    DeclareFormalParameter(parameters->scope, parameter, &classifier);
    if (!duplicate_loc->IsValid()) {
      *duplicate_loc = classifier.duplicate_formal_parameter_error().location;
    }
  }
  DCHECK_EQ(parameters->is_simple, parameters->scope->has_simple_parameters());
}

void Parser::ReindexLiterals(const ParserFormalParameters& parameters) {
  if (function_state_->materialized_literal_count() > 0) {
    AstLiteralReindexer reindexer;

    for (const auto p : parameters.params) {
      if (p.pattern != nullptr) reindexer.Reindex(p.pattern);
      if (p.initializer != nullptr) reindexer.Reindex(p.initializer);
    }

    DCHECK(reindexer.count() <= function_state_->materialized_literal_count());
  }
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

  bool is_generator = IsGeneratorFunction(kind);

  // Anonymous functions were passed either the empty symbol or a null
  // handle as the function name.  Remember if we were passed a non-empty
  // handle to decide whether to invoke function name inference.
  bool should_infer_name = function_name == NULL;

  // We want a non-null handle as the function name.
  if (should_infer_name) {
    function_name = ast_value_factory()->empty_string();
  }

  FunctionLiteral::EagerCompileHint eager_compile_hint =
      function_state_->next_function_is_parenthesized()
          ? FunctionLiteral::kShouldEagerCompile
          : FunctionLiteral::kShouldLazyCompile;

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

  // In addition, we need to distinguish between these cases:
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
  // immediately). The inner function *must* be parsed eagerly to resolve the
  // possible reference to the variable in foo's scope. However, it's possible
  // that it will be compiled lazily.

  // To make this additional case work, both Parser and PreParser implement a
  // logic where only top-level functions will be parsed lazily.
  bool is_lazily_parsed = mode() == PARSE_LAZILY &&
                          scope()->AllowsLazyParsing() &&
                          !function_state_->next_function_is_parenthesized();

  // Determine whether the function body can be discarded after parsing.
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
  // - For asm.js functions the body needs to be available when module
  //   validation is active, because we examine the entire module at once.
  bool use_temp_zone =
      !is_lazily_parsed && FLAG_lazy && !allow_natives() &&
      extension_ == NULL && allow_lazy() &&
      function_type == FunctionLiteral::kDeclaration &&
      eager_compile_hint != FunctionLiteral::kShouldEagerCompile &&
      !(FLAG_validate_asm && scope()->IsAsmModule());

  DeclarationScope* main_scope = nullptr;
  if (use_temp_zone) {
    // This Scope lives in the main Zone; we'll migrate data into it later.
    main_scope = NewFunctionScope(kind);
  }

  ZoneList<Statement*>* body = nullptr;
  int arity = -1;
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  DuplicateFinder duplicate_finder(scanner()->unicode_cache());
  bool should_be_used_once_hint = false;
  bool has_duplicate_parameters;

  {
    // Temporary zones can nest. When we migrate free variables (see below), we
    // need to recreate them in the previous Zone.
    AstNodeFactory previous_zone_ast_node_factory(ast_value_factory());
    previous_zone_ast_node_factory.set_zone(zone());

    // Open a new zone scope, which sets our AstNodeFactory to allocate in the
    // new temporary zone if the preconditions are satisfied, and ensures that
    // the previous zone is always restored after parsing the body. To be able
    // to do scope analysis correctly after full parsing, we migrate needed
    // information from scope into main_scope when the function has been parsed.
    Zone temp_zone(zone()->allocator());
    DiscardableZoneScope zone_scope(this, &temp_zone, use_temp_zone);

    DeclarationScope* scope = NewFunctionScope(kind);
    SetLanguageMode(scope, language_mode);
    if (!use_temp_zone) {
      main_scope = scope;
    } else {
      DCHECK(main_scope->zone() != scope->zone());
    }

    FunctionState function_state(&function_state_, &scope_state_, scope, kind);
#ifdef DEBUG
    scope->SetScopeName(function_name);
#endif
    ExpressionClassifier formals_classifier(this, &duplicate_finder);

    if (is_generator) {
      // For generators, allocating variables in contexts is currently a win
      // because it minimizes the work needed to suspend and resume an
      // activation.  The machine code produced for generators (by full-codegen)
      // relies on this forced context allocation, but not in an essential way.
      this->scope()->ForceContextAllocation();

      // Calling a generator returns a generator object.  That object is stored
      // in a temporary variable, a definition that is used by "yield"
      // expressions. This also marks the FunctionState as a generator.
      Variable* temp =
          NewTemporary(ast_value_factory()->dot_generator_object_string());
      function_state.set_generator_object_variable(temp);
    }

    Expect(Token::LPAREN, CHECK_OK);
    int start_position = scanner()->location().beg_pos;
    this->scope()->set_start_position(start_position);
    ParserFormalParameters formals(scope);
    ParseFormalParameterList(&formals, &formals_classifier, CHECK_OK);
    arity = formals.Arity();
    Expect(Token::RPAREN, CHECK_OK);
    int formals_end_position = scanner()->location().end_pos;

    CheckArityRestrictions(arity, kind, formals.has_rest, start_position,
                           formals_end_position, CHECK_OK);
    Expect(Token::LBRACE, CHECK_OK);
    // Don't include the rest parameter into the function's formal parameter
    // count (esp. the SharedFunctionInfo::internal_formal_parameter_count,
    // which says whether we need to create an arguments adaptor frame).
    if (formals.has_rest) arity--;

    // Eager or lazy parse?
    // If is_lazily_parsed, we'll parse lazy. If we can set a bookmark, we'll
    // pass it to SkipLazyFunctionBody, which may use it to abort lazy
    // parsing if it suspect that wasn't a good idea. If so, or if we didn't
    // try to lazy parse in the first place, we'll have to parse eagerly.
    Scanner::BookmarkScope bookmark(scanner());
    if (is_lazily_parsed) {
      Scanner::BookmarkScope* maybe_bookmark =
          bookmark.Set() ? &bookmark : nullptr;
      SkipLazyFunctionBody(&materialized_literal_count,
                           &expected_property_count, /*CHECK_OK*/ ok,
                           maybe_bookmark);

      materialized_literal_count += formals.materialized_literals_count +
                                    function_state.materialized_literal_count();

      if (bookmark.HasBeenReset()) {
        // Trigger eager (re-)parsing, just below this block.
        is_lazily_parsed = false;

        // This is probably an initialization function. Inform the compiler it
        // should also eager-compile this function, and that we expect it to be
        // used once.
        eager_compile_hint = FunctionLiteral::kShouldEagerCompile;
        should_be_used_once_hint = true;
      }
    }
    if (!is_lazily_parsed) {
      body = ParseEagerFunctionBody(function_name, pos, formals, kind,
                                    function_type, CHECK_OK);

      materialized_literal_count = function_state.materialized_literal_count();
      expected_property_count = function_state.expected_property_count();
      if (use_temp_zone) {
        // If the preconditions are correct the function body should never be
        // accessed, but do this anyway for better behaviour if they're wrong.
        body = nullptr;
      }
    }

    // Parsing the body may change the language mode in our scope.
    language_mode = scope->language_mode();

    // Validate name and parameter names. We can do this only after parsing the
    // function, since the function can declare itself strict.
    CheckFunctionName(language_mode, function_name, function_name_validity,
                      function_name_location, CHECK_OK);
    const bool allow_duplicate_parameters =
        is_sloppy(language_mode) && formals.is_simple && !IsConciseMethod(kind);
    ValidateFormalParameters(&formals_classifier, language_mode,
                             allow_duplicate_parameters, CHECK_OK);

    if (is_strict(language_mode)) {
      CheckStrictOctalLiteral(scope->start_position(), scope->end_position(),
                              CHECK_OK);
      CheckDecimalLiteralWithLeadingZero(use_counts_, scope->start_position(),
                                         scope->end_position());
    }
    CheckConflictingVarDeclarations(scope, CHECK_OK);

    if (body) {
      // If body can be inspected, rewrite queued destructuring assignments
      RewriteDestructuringAssignments();
    }
    has_duplicate_parameters =
      !formals_classifier.is_valid_formal_parameter_list_without_duplicates();

    if (use_temp_zone) {
      DCHECK(main_scope != scope);
      scope->AnalyzePartially(main_scope, &previous_zone_ast_node_factory);
    }
  }  // DiscardableZoneScope goes out of scope.

  FunctionLiteral::ParameterFlag duplicate_parameters =
      has_duplicate_parameters ? FunctionLiteral::kHasDuplicateParameters
                               : FunctionLiteral::kNoDuplicateParameters;

  // Note that the FunctionLiteral needs to be created in the main Zone again.
  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      function_name, main_scope, body, materialized_literal_count,
      expected_property_count, arity, duplicate_parameters, function_type,
      eager_compile_hint, kind, pos);
  function_literal->set_function_token_position(function_token_pos);
  if (should_be_used_once_hint)
    function_literal->set_should_be_used_once_hint();

  if (fni_ != NULL && should_infer_name) fni_->AddFunction(function_literal);
  return function_literal;
}

Expression* Parser::ParseAsyncFunctionExpression(bool* ok) {
  // AsyncFunctionDeclaration ::
  //   async [no LineTerminator here] function ( FormalParameters[Await] )
  //       { AsyncFunctionBody }
  //
  //   async [no LineTerminator here] function BindingIdentifier[Await]
  //       ( FormalParameters[Await] ) { AsyncFunctionBody }
  DCHECK_EQ(scanner()->current_token(), Token::ASYNC);
  int pos = position();
  Expect(Token::FUNCTION, CHECK_OK);
  bool is_strict_reserved = false;
  const AstRawString* name = nullptr;
  FunctionLiteral::FunctionType type = FunctionLiteral::kAnonymousExpression;

  if (peek_any_identifier()) {
    type = FunctionLiteral::kNamedExpression;
    name = ParseIdentifierOrStrictReservedWord(FunctionKind::kAsyncFunction,
                                               &is_strict_reserved, CHECK_OK);
  }
  return ParseFunctionLiteral(name, scanner()->location(),
                              is_strict_reserved ? kFunctionNameIsStrictReserved
                                                 : kFunctionNameValidityUnknown,
                              FunctionKind::kAsyncFunction, pos, type,
                              language_mode(), CHECK_OK);
}

void Parser::SkipLazyFunctionBody(int* materialized_literal_count,
                                  int* expected_property_count, bool* ok,
                                  Scanner::BookmarkScope* bookmark) {
  DCHECK_IMPLIES(bookmark, bookmark->HasBeenSet());
  if (produce_cached_parse_data()) CHECK(log_);

  int function_block_pos = position();
  DeclarationScope* scope = this->scope()->AsDeclarationScope();
  DCHECK(scope->is_function_scope());
  if (consume_cached_parse_data() && !cached_parse_data_->rejected()) {
    // If we have cached data, we use it to skip parsing the function body. The
    // data contains the information we need to construct the lazy function.
    FunctionEntry entry =
        cached_parse_data_->GetFunctionEntry(function_block_pos);
    // Check that cached data is valid. If not, mark it as invalid (the embedder
    // handles it). Note that end position greater than end of stream is safe,
    // and hard to check.
    if (entry.is_valid() && entry.end_pos() > function_block_pos) {
      scanner()->SeekForward(entry.end_pos() - 1);

      scope->set_end_position(entry.end_pos());
      Expect(Token::RBRACE, CHECK_OK_VOID);
      total_preparse_skipped_ += scope->end_position() - function_block_pos;
      *materialized_literal_count = entry.literal_count();
      *expected_property_count = entry.property_count();
      SetLanguageMode(scope, entry.language_mode());
      if (entry.uses_super_property()) scope->RecordSuperPropertyUsage();
      if (entry.calls_eval()) scope->RecordEvalCall();
      return;
    }
    cached_parse_data_->Reject();
  }
  // With no cached data, we partially parse the function, without building an
  // AST. This gathers the data needed to build a lazy function.
  SingletonLogger logger;
  PreParser::PreParseResult result =
      ParseLazyFunctionBodyWithPreParser(&logger, bookmark);
  if (bookmark && bookmark->HasBeenReset()) {
    return;  // Return immediately if pre-parser devided to abort parsing.
  }
  if (result == PreParser::kPreParseStackOverflow) {
    // Propagate stack overflow.
    set_stack_overflow();
    *ok = false;
    return;
  }
  if (logger.has_error()) {
    ReportMessageAt(Scanner::Location(logger.start(), logger.end()),
                    logger.message(), logger.argument_opt(),
                    logger.error_type());
    *ok = false;
    return;
  }
  scope->set_end_position(logger.end());
  Expect(Token::RBRACE, CHECK_OK_VOID);
  total_preparse_skipped_ += scope->end_position() - function_block_pos;
  *materialized_literal_count = logger.literals();
  *expected_property_count = logger.properties();
  SetLanguageMode(scope, logger.language_mode());
  if (logger.uses_super_property()) scope->RecordSuperPropertyUsage();
  if (logger.calls_eval()) scope->RecordEvalCall();
  if (produce_cached_parse_data()) {
    DCHECK(log_);
    // Position right after terminal '}'.
    int body_end = scanner()->location().end_pos;
    log_->LogFunction(function_block_pos, body_end, *materialized_literal_count,
                      *expected_property_count, language_mode(),
                      scope->uses_super_property(), scope->calls_eval());
  }
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
  for (int i = 0; i < parameters.params.length(); ++i) {
    auto parameter = parameters.params[i];
    if (parameter.is_rest && parameter.pattern->IsVariableProxy()) break;
    DeclarationDescriptor descriptor;
    descriptor.declaration_kind = DeclarationDescriptor::PARAMETER;
    descriptor.parser = this;
    descriptor.scope = scope();
    descriptor.hoist_scope = nullptr;
    descriptor.mode = LET;
    descriptor.declaration_pos = parameter.pattern->position();
    // The position that will be used by the AssignmentExpression
    // which copies from the temp parameter to the pattern.
    //
    // TODO(adamk): Should this be kNoSourcePosition, since
    // it's just copying from a temp var to the real param var?
    descriptor.initialization_pos = parameter.pattern->position();
    // The initializer position which will end up in,
    // Variable::initializer_position(), used for hole check elimination.
    int initializer_position = parameter.pattern->position();
    Expression* initial_value =
        factory()->NewVariableProxy(parameters.scope->parameter(i));
    if (parameter.initializer != nullptr) {
      // IS_UNDEFINED($param) ? initializer : $param

      // Ensure initializer is rewritten
      RewriteParameterInitializer(parameter.initializer, scope());

      auto condition = factory()->NewCompareOperation(
          Token::EQ_STRICT,
          factory()->NewVariableProxy(parameters.scope->parameter(i)),
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);
      initial_value = factory()->NewConditional(
          condition, parameter.initializer, initial_value, kNoSourcePosition);
      descriptor.initialization_pos = parameter.initializer->position();
      initializer_position = parameter.initializer_end_position;
    }

    Scope* param_scope = scope();
    Block* param_block = init_block;
    if (!parameter.is_simple() && scope()->calls_sloppy_eval()) {
      param_scope = NewVarblockScope();
      param_scope->set_start_position(descriptor.initialization_pos);
      param_scope->set_end_position(parameter.initializer_end_position);
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
        parameter.pattern, initializer_position, initial_value);
    PatternRewriter::DeclareAndInitializeVariables(param_block, &descriptor,
                                                   &decl, nullptr, CHECK_OK);

    if (param_block != init_block) {
      param_scope = block_state.FinalizedBlockScope();
      if (param_scope != nullptr) {
        CheckConflictingVarDeclarations(param_scope, CHECK_OK);
      }
      init_block->statements()->Add(param_block, zone());
    }
  }
  return init_block;
}

Block* Parser::BuildRejectPromiseOnException(Block* block) {
  // try { <block> } catch (error) { return Promise.reject(error); }
  Block* try_block = block;
  Scope* catch_scope = NewScope(CATCH_SCOPE);
  catch_scope->set_is_hidden();
  Variable* catch_variable =
      catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(), VAR,
                                kCreatedInitialized, Variable::NORMAL);
  Block* catch_block = factory()->NewBlock(nullptr, 1, true, kNoSourcePosition);

  Expression* promise_reject = BuildPromiseReject(
      factory()->NewVariableProxy(catch_variable), kNoSourcePosition);

  ReturnStatement* return_promise_reject =
      factory()->NewReturnStatement(promise_reject, kNoSourcePosition);
  catch_block->statements()->Add(return_promise_reject, zone());
  TryStatement* try_catch_statement = factory()->NewTryCatchStatement(
      try_block, catch_scope, catch_variable, catch_block, kNoSourcePosition);

  block = factory()->NewBlock(nullptr, 1, true, kNoSourcePosition);
  block->statements()->Add(try_catch_statement, zone());
  return block;
}

Expression* Parser::BuildCreateJSGeneratorObject(int pos, FunctionKind kind) {
  DCHECK_NOT_NULL(function_state_->generator_object_variable());
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(2, zone());
  args->Add(factory()->NewThisFunction(pos), zone());
  args->Add(IsArrowFunction(kind) ? GetLiteralUndefined(pos)
                                  : ThisExpression(kNoSourcePosition),
            zone());
  return factory()->NewCallRuntime(Runtime::kCreateJSGeneratorObject, args,
                                   pos);
}

Expression* Parser::BuildPromiseResolve(Expression* value, int pos) {
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(1, zone());
  args->Add(value, zone());
  return factory()->NewCallRuntime(Context::PROMISE_CREATE_RESOLVED_INDEX, args,
                                   pos);
}

Expression* Parser::BuildPromiseReject(Expression* value, int pos) {
  ZoneList<Expression*>* args = new (zone()) ZoneList<Expression*>(1, zone());
  args->Add(value, zone());
  return factory()->NewCallRuntime(Context::PROMISE_CREATE_REJECTED_INDEX, args,
                                   pos);
}

ZoneList<Statement*>* Parser::ParseEagerFunctionBody(
    const AstRawString* function_name, int pos,
    const ParserFormalParameters& parameters, FunctionKind kind,
    FunctionLiteral::FunctionType function_type, bool* ok) {
  // Everything inside an eagerly parsed function will be parsed eagerly
  // (see comment above).
  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);
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

      {
        Expression* allocation = BuildCreateJSGeneratorObject(pos, kind);
        VariableProxy* init_proxy = factory()->NewVariableProxy(
            function_state_->generator_object_variable());
        Assignment* assignment = factory()->NewAssignment(
            Token::INIT, init_proxy, allocation, kNoSourcePosition);
        VariableProxy* get_proxy = factory()->NewVariableProxy(
            function_state_->generator_object_variable());
        // The position of the yield is important for reporting the exception
        // caused by calling the .throw method on a generator suspended at the
        // initial yield (i.e. right after generator instantiation).
        Yield* yield = factory()->NewYield(get_proxy, assignment,
                                           scope()->start_position(),
                                           Yield::kOnExceptionThrow);
        try_block->statements()->Add(
            factory()->NewExpressionStatement(yield, kNoSourcePosition),
            zone());
      }

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
      DesugarAsyncFunctionBody(function_name, inner_scope, body, nullptr, kind,
                               FunctionBodyType::kNormal, accept_IN, pos,
                               CHECK_OK);
    } else {
      ParseStatementList(body, Token::RBRACE, CHECK_OK);
    }

    if (IsSubclassConstructor(kind)) {
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
      InsertSloppyBlockFunctionVarBindings(inner_scope, function_scope,
                                           CHECK_OK);
    }

    if (IsAsyncFunction(kind)) {
      init_block = BuildRejectPromiseOnException(init_block);
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
      InsertSloppyBlockFunctionVarBindings(function_scope, nullptr, CHECK_OK);
    }
  }

  if (function_type == FunctionLiteral::kNamedExpression) {
    // Now that we know the language mode, we can create the const assignment
    // in the previously reserved spot.
    DCHECK_EQ(function_scope, scope());
    Variable* fvar = function_scope->DeclareFunctionVar(function_name);
    VariableProxy* fproxy = factory()->NewVariableProxy(fvar);
    result->Set(kFunctionNameAssignmentIndex,
                factory()->NewExpressionStatement(
                    factory()->NewAssignment(Token::INIT, fproxy,
                                             factory()->NewThisFunction(pos),
                                             kNoSourcePosition),
                    kNoSourcePosition));
  }

  MarkCollectedTailCallExpressions();
  return result;
}


PreParser::PreParseResult Parser::ParseLazyFunctionBodyWithPreParser(
    SingletonLogger* logger, Scanner::BookmarkScope* bookmark) {
  // This function may be called on a background thread too; record only the
  // main thread preparse times.
  if (pre_parse_timer_ != NULL) {
    pre_parse_timer_->Start();
  }
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.PreParse");

  DCHECK_EQ(Token::LBRACE, scanner()->current_token());

  if (reusable_preparser_ == NULL) {
    reusable_preparser_ = new PreParser(zone(), &scanner_, ast_value_factory(),
                                        NULL, stack_limit_);
    reusable_preparser_->set_allow_lazy(true);
#define SET_ALLOW(name) reusable_preparser_->set_allow_##name(allow_##name());
    SET_ALLOW(natives);
    SET_ALLOW(harmony_do_expressions);
    SET_ALLOW(harmony_for_in);
    SET_ALLOW(harmony_function_sent);
    SET_ALLOW(harmony_restrictive_declarations);
    SET_ALLOW(harmony_async_await);
    SET_ALLOW(harmony_trailing_commas);
#undef SET_ALLOW
  }
  PreParser::PreParseResult result = reusable_preparser_->PreParseLazyFunction(
      language_mode(), function_state_->kind(),
      scope()->AsDeclarationScope()->has_simple_parameters(), parsing_module_,
      logger, bookmark, use_counts_);
  if (pre_parse_timer_ != NULL) {
    pre_parse_timer_->Stop();
  }
  return result;
}

Expression* Parser::ParseClassLiteral(ExpressionClassifier* classifier,
                                      const AstRawString* name,
                                      Scanner::Location class_name_location,
                                      bool name_is_strict_reserved, int pos,
                                      bool* ok) {
  // All parts of a ClassDeclaration and ClassExpression are strict code.
  if (name_is_strict_reserved) {
    ReportMessageAt(class_name_location,
                    MessageTemplate::kUnexpectedStrictReserved);
    *ok = false;
    return nullptr;
  }
  if (IsEvalOrArguments(name)) {
    ReportMessageAt(class_name_location, MessageTemplate::kStrictEvalArguments);
    *ok = false;
    return nullptr;
  }

  BlockState block_state(&scope_state_);
  RaiseLanguageMode(STRICT);
#ifdef DEBUG
  scope()->SetScopeName(name);
#endif

  VariableProxy* proxy = nullptr;
  if (name != nullptr) {
    proxy = NewUnresolved(name);
    // TODO(verwaest): declare via block_state.
    Declaration* declaration =
        factory()->NewVariableDeclaration(proxy, block_state.scope(), pos);
    Declare(declaration, DeclarationDescriptor::NORMAL, CONST,
            DefaultInitializationFlag(CONST), CHECK_OK);
  }

  Expression* extends = nullptr;
  if (Check(Token::EXTENDS)) {
    block_state.set_start_position(scanner()->location().end_pos);
    ExpressionClassifier extends_classifier(this);
    extends = ParseLeftHandSideExpression(&extends_classifier, CHECK_OK);
    CheckNoTailCallExpressions(&extends_classifier, CHECK_OK);
    RewriteNonPattern(&extends_classifier, CHECK_OK);
    if (classifier != nullptr) {
      classifier->Accumulate(&extends_classifier,
                             ExpressionClassifier::ExpressionProductions);
    }
  } else {
    block_state.set_start_position(scanner()->location().end_pos);
  }


  ClassLiteralChecker checker(this);
  ZoneList<ObjectLiteral::Property*>* properties = NewPropertyList(4);
  FunctionLiteral* constructor = nullptr;
  bool has_seen_constructor = false;

  Expect(Token::LBRACE, CHECK_OK);

  const bool has_extends = extends != nullptr;
  while (peek() != Token::RBRACE) {
    if (Check(Token::SEMICOLON)) continue;
    FuncNameInferrer::State fni_state(fni_);
    const bool in_class = true;
    bool is_computed_name = false;  // Classes do not care about computed
                                    // property names here.
    ExpressionClassifier property_classifier(this);
    const AstRawString* property_name = nullptr;
    ObjectLiteral::Property* property = ParsePropertyDefinition(
        &checker, in_class, has_extends, MethodKind::kNormal, &is_computed_name,
        &has_seen_constructor, &property_classifier, &property_name, CHECK_OK);
    RewriteNonPattern(&property_classifier, CHECK_OK);
    if (classifier != nullptr) {
      classifier->Accumulate(&property_classifier,
                             ExpressionClassifier::ExpressionProductions);
    }

    if (has_seen_constructor && constructor == nullptr) {
      constructor = GetPropertyValue(property)->AsFunctionLiteral();
      DCHECK_NOT_NULL(constructor);
      constructor->set_raw_name(
          name != nullptr ? name : ast_value_factory()->empty_string());
    } else {
      properties->Add(property, zone());
    }

    if (fni_ != nullptr) fni_->Infer();

    if (property_name != ast_value_factory()->constructor_string()) {
      SetFunctionNameFromPropertyName(property, property_name);
    }
  }

  Expect(Token::RBRACE, CHECK_OK);
  int end_pos = scanner()->location().end_pos;

  if (constructor == nullptr) {
    constructor = DefaultConstructor(name, has_extends, pos, end_pos,
                                     block_state.language_mode());
  }

  // Note that we do not finalize this block scope because it is
  // used as a sentinel value indicating an anonymous class.
  block_state.set_end_position(end_pos);

  if (name != nullptr) {
    DCHECK_NOT_NULL(proxy);
    proxy->var()->set_initializer_position(end_pos);
  }

  Block* do_block = factory()->NewBlock(nullptr, 1, false, pos);
  Variable* result_var = NewTemporary(ast_value_factory()->empty_string());
  do_block->set_scope(block_state.FinalizedBlockScope());
  DoExpression* do_expr = factory()->NewDoExpression(do_block, result_var, pos);

  ClassLiteral* class_literal = factory()->NewClassLiteral(
      proxy, extends, constructor, properties, pos, end_pos);

  do_block->statements()->Add(
      factory()->NewExpressionStatement(class_literal, pos), zone());
  do_expr->set_represented_function(constructor);
  Rewriter::Rewrite(this, GetClosureScope(), do_expr, ast_value_factory());

  return do_expr;
}


Expression* Parser::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments

  int pos = peek_position();
  Expect(Token::MOD, CHECK_OK);
  // Allow "eval" or "arguments" for backward compatibility.
  const AstRawString* name = ParseIdentifier(kAllowRestrictedIdentifiers,
                                             CHECK_OK);
  Scanner::Location spread_pos;
  ExpressionClassifier classifier(this);
  ZoneList<Expression*>* args =
      ParseArguments(&spread_pos, &classifier, CHECK_OK);

  DCHECK(!spread_pos.IsValid());

  if (extension_ != NULL) {
    // The extension structures are only accessible while parsing the
    // very first time not when reparsing because of lazy compilation.
    GetClosureScope()->ForceEagerCompilation();
  }

  const Runtime::Function* function = Runtime::FunctionForName(name->string());

  if (function != NULL) {
    // Check for possible name clash.
    DCHECK_EQ(Context::kNotFound,
              Context::IntrinsicIndexForName(name->string()));
    // Check for built-in IS_VAR macro.
    if (function->function_id == Runtime::kIS_VAR) {
      DCHECK_EQ(Runtime::RUNTIME, function->intrinsic_type);
      // %IS_VAR(x) evaluates to x if x is a variable,
      // leads to a parse error otherwise.  Could be implemented as an
      // inline function %_IS_VAR(x) to eliminate this special case.
      if (args->length() == 1 && args->at(0)->AsVariableProxy() != NULL) {
        return args->at(0);
      } else {
        ReportMessage(MessageTemplate::kNotIsvar);
        *ok = false;
        return NULL;
      }
    }

    // Check that the expected number of arguments are being passed.
    if (function->nargs != -1 && function->nargs != args->length()) {
      ReportMessage(MessageTemplate::kRuntimeWrongNumArgs);
      *ok = false;
      return NULL;
    }

    return factory()->NewCallRuntime(function, args, pos);
  }

  int context_index = Context::IntrinsicIndexForName(name->string());

  // Check that the function is defined.
  if (context_index == Context::kNotFound) {
    ReportMessage(MessageTemplate::kNotDefined, name);
    *ok = false;
    return NULL;
  }

  return factory()->NewCallRuntime(context_index, args, pos);
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
  ZoneList<Declaration*>* decls = inner_scope->declarations();
  BlockState block_state(&scope_state_, inner_scope);
  for (int i = 0; i < decls->length(); ++i) {
    Declaration* decl = decls->at(i);
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

void Parser::InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope,
                                                  Scope* complex_params_scope,
                                                  bool* ok) {
  // For each variable which is used as a function declaration in a sloppy
  // block,
  SloppyBlockFunctionMap* map = scope->sloppy_block_function_map();
  for (ZoneHashMap::Entry* p = map->Start(); p != nullptr; p = map->Next(p)) {
    AstRawString* name = static_cast<AstRawString*>(p->key);

    // If the variable wouldn't conflict with a lexical declaration
    // or parameter,

    // Check if there's a conflict with a parameter.
    // This depends on the fact that functions always have a scope solely to
    // hold complex parameters, and the names local to that scope are
    // precisely the names of the parameters. IsDeclaredParameter(name) does
    // not hold for names declared by complex parameters, nor are those
    // bindings necessarily declared lexically, so we have to check for them
    // explicitly. On the other hand, if there are not complex parameters,
    // it is sufficient to just check IsDeclaredParameter.
    if (complex_params_scope != nullptr) {
      if (complex_params_scope->LookupLocal(name) != nullptr) {
        continue;
      }
    } else {
      if (scope->IsDeclaredParameter(name)) {
        continue;
      }
    }

    bool var_created = false;

    // Write in assignments to var for each block-scoped function declaration
    auto delegates = static_cast<SloppyBlockFunctionStatement*>(p->value);

    DeclarationScope* decl_scope = scope;
    while (decl_scope->is_eval_scope()) {
      decl_scope = decl_scope->outer_scope()->GetDeclarationScope();
    }
    Scope* outer_scope = decl_scope->outer_scope();

    for (SloppyBlockFunctionStatement* delegate = delegates;
         delegate != nullptr; delegate = delegate->next()) {
      // Check if there's a conflict with a lexical declaration
      Scope* query_scope = delegate->scope()->outer_scope();
      Variable* var = nullptr;
      bool should_hoist = true;

      // Note that we perform this loop for each delegate named 'name',
      // which may duplicate work if those delegates share scopes.
      // It is not sufficient to just do a Lookup on query_scope: for
      // example, that does not prevent hoisting of the function in
      // `{ let e; try {} catch (e) { function e(){} } }`
      do {
        var = query_scope->LookupLocal(name);
        if (var != nullptr && IsLexicalVariableMode(var->mode())) {
          should_hoist = false;
          break;
        }
        query_scope = query_scope->outer_scope();
      } while (query_scope != outer_scope);

      if (!should_hoist) continue;

      // Declare a var-style binding for the function in the outer scope
      if (!var_created) {
        var_created = true;
        VariableProxy* proxy = scope->NewUnresolved(factory(), name);
        Declaration* declaration =
            factory()->NewVariableDeclaration(proxy, scope, kNoSourcePosition);
        Declare(declaration, DeclarationDescriptor::NORMAL, VAR,
                DefaultInitializationFlag(VAR), ok, scope);
        DCHECK(ok);  // Based on the preceding check, this should not fail
        if (!ok) return;
      }

      // Read from the local lexical scope and write to the function scope
      VariableProxy* to = scope->NewUnresolved(factory(), name);
      VariableProxy* from = delegate->scope()->NewUnresolved(factory(), name);
      Expression* assignment =
          factory()->NewAssignment(Token::ASSIGN, to, from, kNoSourcePosition);
      Statement* statement =
          factory()->NewExpressionStatement(assignment, kNoSourcePosition);
      delegate->set_statement(statement);
    }
  }
}


// ----------------------------------------------------------------------------
// Parser support

bool Parser::TargetStackContainsLabel(const AstRawString* label) {
  for (Target* t = target_stack_; t != NULL; t = t->previous()) {
    if (ContainsLabel(t->statement()->labels(), label)) return true;
  }
  return false;
}


BreakableStatement* Parser::LookupBreakTarget(const AstRawString* label,
                                              bool* ok) {
  bool anonymous = label == NULL;
  for (Target* t = target_stack_; t != NULL; t = t->previous()) {
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
  for (Target* t = target_stack_; t != NULL; t = t->previous()) {
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
  // Internalize strings.
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
}


// ----------------------------------------------------------------------------
// The Parser interface.


bool Parser::ParseStatic(ParseInfo* info) {
  Parser parser(info);
  if (parser.Parse(info)) {
    info->set_language_mode(info->literal()->language_mode());
    return true;
  }
  return false;
}


bool Parser::Parse(ParseInfo* info) {
  DCHECK(info->literal() == NULL);
  FunctionLiteral* result = NULL;
  // Ok to use Isolate here; this function is only called in the main thread.
  DCHECK(parsing_on_main_thread_);
  Isolate* isolate = info->isolate();
  pre_parse_timer_ = isolate->counters()->pre_parse();
  if (FLAG_trace_parse || allow_natives() || extension_ != NULL) {
    // If intrinsics are allowed, the Parser cannot operate independent of the
    // V8 heap because of Runtime. Tell the string table to internalize strings
    // and values right after they're created.
    ast_value_factory()->Internalize(isolate);
  }

  if (info->is_lazy()) {
    DCHECK(!info->is_eval());
    if (info->shared_info()->is_function()) {
      result = ParseLazy(isolate, info);
    } else {
      result = ParseProgram(isolate, info);
    }
  } else {
    SetCachedData(info);
    result = ParseProgram(isolate, info);
  }
  info->set_literal(result);

  Internalize(isolate, info->script(), result == NULL);
  DCHECK(ast_value_factory()->IsInternalized());
  return (result != NULL);
}


void Parser::ParseOnBackground(ParseInfo* info) {
  parsing_on_main_thread_ = false;

  DCHECK(info->literal() == NULL);
  FunctionLiteral* result = NULL;

  CompleteParserRecorder recorder;
  if (produce_cached_parse_data()) log_ = &recorder;

  std::unique_ptr<Utf16CharacterStream> stream;
  Utf16CharacterStream* stream_ptr;
  if (info->character_stream()) {
    DCHECK(info->source_stream() == nullptr);
    stream_ptr = info->character_stream();
  } else {
    DCHECK(info->character_stream() == nullptr);
    stream.reset(new ExternalStreamingStream(info->source_stream(),
                                             info->source_stream_encoding()));
    stream_ptr = stream.get();
  }
  DCHECK(info->context().is_null() || info->context()->IsNativeContext());

  DCHECK(original_scope_);

  // When streaming, we don't know the length of the source until we have parsed
  // it. The raw data can be UTF-8, so we wouldn't know the source length until
  // we have decoded it anyway even if we knew the raw data length (which we
  // don't). We work around this by storing all the scopes which need their end
  // position set at the end of the script (the top scope and possible eval
  // scopes) and set their end position after we know the script length.
  if (info->is_lazy()) {
    result = DoParseLazy(info, info->function_name(), stream_ptr);
  } else {
    fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());
    scanner_.Initialize(stream_ptr);
    result = DoParseProgram(info);
  }

  info->set_literal(result);

  // We cannot internalize on a background thread; a foreground task will take
  // care of calling Parser::Internalize just before compilation.

  if (produce_cached_parse_data()) {
    if (result != NULL) *info->cached_data() = recorder.GetScriptData();
    log_ = NULL;
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

    // Ensure hash is suitable as a Smi value
    Smi* hash_obj = Smi::cast(Internals::IntToSmi(static_cast<int>(hash)));
    args->Add(factory()->NewSmiLiteral(hash_obj->value(), pos), zone());

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


ZoneList<v8::internal::Expression*>* Parser::PrepareSpreadArguments(
    ZoneList<v8::internal::Expression*>* list) {
  ZoneList<v8::internal::Expression*>* args =
      new (zone()) ZoneList<v8::internal::Expression*>(1, zone());
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
    args->Add(factory()->NewCallRuntime(Context::SPREAD_ITERABLE_INDEX,
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
        ZoneList<v8::internal::Expression*>* unspread =
            new (zone()) ZoneList<v8::internal::Expression*>(1, zone());

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
      ZoneList<v8::internal::Expression*>* spread_list =
          new (zone()) ZoneList<v8::internal::Expression*>(1, zone());
      spread_list->Add(list->at(i++)->AsSpread()->expression(), zone());
      args->Add(factory()->NewCallRuntime(Context::SPREAD_ITERABLE_INDEX,
                                          spread_list, kNoSourcePosition),
                zone());
    }

    list = new (zone()) ZoneList<v8::internal::Expression*>(1, zone());
    list->Add(factory()->NewCallRuntime(Context::SPREAD_ARGUMENTS_INDEX, args,
                                        kNoSourcePosition),
              zone());
    return list;
  }
  UNREACHABLE();
}


Expression* Parser::SpreadCall(Expression* function,
                               ZoneList<v8::internal::Expression*>* args,
                               int pos) {
  if (function->IsSuperCallReference()) {
    // Super calls
    // $super_constructor = %_GetSuperConstructor(<this-function>)
    // %reflect_construct($super_constructor, args, new.target)
    ZoneList<Expression*>* tmp = new (zone()) ZoneList<Expression*>(1, zone());
    tmp->Add(function->AsSuperCallReference()->this_function_var(), zone());
    Expression* super_constructor = factory()->NewCallRuntime(
        Runtime::kInlineGetSuperConstructor, tmp, pos);
    args->InsertAt(0, super_constructor, zone());
    args->Add(function->AsSuperCallReference()->new_target_var(), zone());
    return factory()->NewCallRuntime(Context::REFLECT_CONSTRUCT_INDEX, args,
                                     pos);
  } else {
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
                                  ZoneList<v8::internal::Expression*>* args,
                                  int pos) {
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


void Parser::RaiseLanguageMode(LanguageMode mode) {
  LanguageMode old = scope()->language_mode();
  SetLanguageMode(scope(), old > mode ? old : mode);
}

void Parser::MarkCollectedTailCallExpressions() {
  const ZoneList<Expression*>& tail_call_expressions =
      function_state_->tail_call_expressions().expressions();
  for (int i = 0; i < tail_call_expressions.length(); ++i) {
    Expression* expression = tail_call_expressions[i];
    // If only FLAG_harmony_explicit_tailcalls is enabled then expression
    // must be a Call expression.
    DCHECK(FLAG_harmony_tailcalls || !FLAG_harmony_explicit_tailcalls ||
           expression->IsCall());
    MarkTailPosition(expression);
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

Expression* Parser::RewriteAwaitExpression(Expression* value, int await_pos) {
  // yield %AsyncFunctionAwait(.generator_object, <operand>)
  Variable* generator_object_variable =
      function_state_->generator_object_variable();

  // If generator_object_variable is null,
  if (!generator_object_variable) return value;

  const int nopos = kNoSourcePosition;

  Variable* temp_var = NewTemporary(ast_value_factory()->empty_string());
  VariableProxy* temp_proxy = factory()->NewVariableProxy(temp_var);
  Block* do_block = factory()->NewBlock(nullptr, 2, false, nopos);

  // Wrap value evaluation to provide a break location.
  Expression* value_assignment =
      factory()->NewAssignment(Token::ASSIGN, temp_proxy, value, nopos);
  do_block->statements()->Add(
      factory()->NewExpressionStatement(value_assignment, value->position()),
      zone());

  ZoneList<Expression*>* async_function_await_args =
      new (zone()) ZoneList<Expression*>(2, zone());
  Expression* generator_object =
      factory()->NewVariableProxy(generator_object_variable);
  async_function_await_args->Add(generator_object, zone());
  async_function_await_args->Add(temp_proxy, zone());
  Expression* async_function_await = factory()->NewCallRuntime(
      Context::ASYNC_FUNCTION_AWAIT_INDEX, async_function_await_args, nopos);
  // Wrap await to provide a break location between value evaluation and yield.
  Expression* await_assignment = factory()->NewAssignment(
      Token::ASSIGN, temp_proxy, async_function_await, nopos);
  do_block->statements()->Add(
      factory()->NewExpressionStatement(await_assignment, await_pos), zone());
  Expression* do_expr = factory()->NewDoExpression(do_block, temp_var, nopos);

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

  void VisitObjectLiteralProperty(ObjectLiteralProperty* property) override {
    if (property == nullptr) return;
    // Do not rewrite (computed) key expressions
    AST_REWRITE_PROPERTY(Expression, property, value);
  }

  Parser* parser_;
};


void Parser::RewriteNonPattern(ExpressionClassifier* classifier, bool* ok) {
  ValidateExpression(classifier, CHECK_OK_VOID);
  auto non_patterns_to_rewrite = function_state_->non_patterns_to_rewrite();
  int begin = classifier->GetNonPatternBegin();
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
      PatternRewriter::RewriteDestructuringAssignment(this, to_rewrite,
                                                      pair.scope);
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
    result = ExpressionFromIdentifier(lhs->raw_name(), lhs->position(),
                                      lhs->end_position());
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
      ZoneList<Expression*>* append_element_args = NewExpressionList(2);
      append_element_args->Add(factory()->NewVariableProxy(result), zone());
      append_element_args->Add(value, zone());
      do_block->statements()->Add(
          factory()->NewExpressionStatement(
              factory()->NewCallRuntime(Runtime::kAppendElement,
                                        append_element_args, kNoSourcePosition),
              kNoSourcePosition),
          zone());
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

void Parser::SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                             const AstRawString* name) {
  Expression* value = property->value();

  // Computed name setting must happen at runtime.
  if (property->is_computed_name()) return;

  // Getter and setter names are handled here because their names
  // change in ES2015, even though they are not anonymous.
  auto function = value->AsFunctionLiteral();
  if (function != nullptr) {
    bool is_getter = property->kind() == ObjectLiteralProperty::GETTER;
    bool is_setter = property->kind() == ObjectLiteralProperty::SETTER;
    if (is_getter || is_setter) {
      DCHECK_NOT_NULL(name);
      const AstRawString* prefix =
          is_getter ? ast_value_factory()->get_space_string()
                    : ast_value_factory()->set_space_string();
      function->set_raw_name(ast_value_factory()->NewConsString(prefix, name));
      return;
    }
  }

  // Ignore "__proto__" as a name when it's being used to set the [[Prototype]]
  // of an object literal.
  if (property->kind() == ObjectLiteralProperty::PROTOTYPE) return;

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
//     let iterator = iterable[Symbol.iterator]();
//     if (!IS_RECEIVER(iterator)) throw MakeTypeError(kSymbolIteratorInvalid);
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

  // let iterator = iterable[Symbol.iterator];
  Variable* var_iterator = NewTemporary(ast_value_factory()->empty_string());
  Statement* get_iterator;
  {
    Expression* iterator = GetIterator(iterable, nopos);
    Expression* iterator_proxy = factory()->NewVariableProxy(var_iterator);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, iterator_proxy, iterator, nopos);
    get_iterator = factory()->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_RECEIVER(iterator)) throw MakeTypeError(kSymbolIteratorInvalid);
  Statement* validate_iterator;
  {
    Expression* is_receiver_call;
    {
      auto args = new (zone()) ZoneList<Expression*>(1, zone());
      args->Add(factory()->NewVariableProxy(var_iterator), zone());
      is_receiver_call =
          factory()->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      Expression* call =
          NewThrowTypeError(MessageTemplate::kSymbolIteratorInvalid,
                            ast_value_factory()->empty_string(), nopos);
      throw_call = factory()->NewExpressionStatement(call, nopos);
    }

    validate_iterator = factory()->NewIfStatement(
        is_receiver_call, factory()->NewEmptyStatement(nopos), throw_call,
        nopos);
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
        then->statements(), var_iterator,
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
    Variable* catch_variable =
        catch_scope->DeclareLocal(name, VAR, kCreatedInitialized,
                                               Variable::NORMAL);

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

    Block* do_block_ = factory()->NewBlock(nullptr, 7, true, nopos);
    do_block_->statements()->Add(initialize_input, zone());
    do_block_->statements()->Add(initialize_mode, zone());
    do_block_->statements()->Add(initialize_output, zone());
    do_block_->statements()->Add(get_iterator, zone());
    do_block_->statements()->Add(validate_iterator, zone());
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

void Parser::FinalizeIteratorUse(Variable* completion, Expression* condition,
                                 Variable* iter, Block* iterator_use,
                                 Block* target) {
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
    BuildIteratorCloseForCompletion(block->statements(), iter, proxy);
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
    Scope* catch_scope = NewScopeWithParent(scope(), CATCH_SCOPE);
    Variable* catch_variable =
        catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(), VAR,
                                  kCreatedInitialized, Variable::NORMAL);
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

void Parser::BuildIteratorCloseForCompletion(ZoneList<Statement*>* statements,
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

    Scope* catch_scope = NewScope(CATCH_SCOPE);
    Variable* catch_variable =
        catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(), VAR,
                                  kCreatedInitialized, Variable::NORMAL);
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
  //     if (!(completion === kNormalCompletion || IS_UNDEFINED(#iterator))) {
  //       #BuildIteratorCloseForCompletion(#iterator, completion)
  //     }
  //   }
  //
  // Note that the loop's body and its assign_each already contain appropriate
  // assignments to completion (see InitializeForOfStatement).
  //

  const int nopos = kNoSourcePosition;

  // !(completion === kNormalCompletion || IS_UNDEFINED(#iterator))
  Expression* closing_condition;
  {
    Expression* lhs = factory()->NewCompareOperation(
        Token::EQ_STRICT, factory()->NewVariableProxy(var_completion),
        factory()->NewSmiLiteral(Parser::kNormalCompletion, nopos), nopos);
    Expression* rhs = factory()->NewCompareOperation(
        Token::EQ_STRICT, factory()->NewVariableProxy(loop->iterator()),
        factory()->NewUndefinedLiteral(nopos), nopos);
    closing_condition = factory()->NewUnaryOperation(
        Token::NOT, factory()->NewBinaryOperation(Token::OR, lhs, rhs, nopos),
        nopos);
  }

  Block* final_loop = factory()->NewBlock(nullptr, 2, false, nopos);
  {
    Block* try_block = factory()->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(loop, zone());

    FinalizeIteratorUse(var_completion, closing_condition, loop->iterator(),
                        try_block, final_loop);
  }

  return final_loop;
}

#ifdef DEBUG
void Parser::Print(AstNode* node) {
  ast_value_factory()->Internalize(Isolate::Current());
  node->Print(Isolate::Current());
}
#endif  // DEBUG

#undef CHECK_OK
#undef CHECK_OK_VOID
#undef CHECK_FAILED

}  // namespace internal
}  // namespace v8
