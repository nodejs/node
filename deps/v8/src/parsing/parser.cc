// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parser.h"

#include "src/api.h"
#include "src/ast/ast.h"
#include "src/ast/ast-expression-rewriter.h"
#include "src/ast/ast-expression-visitor.h"
#include "src/ast/ast-literal-reindexer.h"
#include "src/ast/scopeinfo.h"
#include "src/bailout-reason.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/char-predicates-inl.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/messages.h"
#include "src/parsing/parameter-initializer-rewriter.h"
#include "src/parsing/parser-base.h"
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


ParseInfo::ParseInfo(Zone* zone)
    : zone_(zone),
      flags_(0),
      source_stream_(nullptr),
      source_stream_encoding_(ScriptCompiler::StreamedSource::ONE_BYTE),
      extension_(nullptr),
      compile_options_(ScriptCompiler::kNoCompileOptions),
      script_scope_(nullptr),
      unicode_cache_(nullptr),
      stack_limit_(0),
      hash_seed_(0),
      cached_data_(nullptr),
      ast_value_factory_(nullptr),
      literal_(nullptr),
      scope_(nullptr) {}


ParseInfo::ParseInfo(Zone* zone, Handle<JSFunction> function)
    : ParseInfo(zone, Handle<SharedFunctionInfo>(function->shared())) {
  set_closure(function);
  set_context(Handle<Context>(function->context()));
}


ParseInfo::ParseInfo(Zone* zone, Handle<SharedFunctionInfo> shared)
    : ParseInfo(zone) {
  isolate_ = shared->GetIsolate();

  set_lazy();
  set_hash_seed(isolate_->heap()->HashSeed());
  set_stack_limit(isolate_->stack_guard()->real_climit());
  set_unicode_cache(isolate_->unicode_cache());
  set_language_mode(shared->language_mode());
  set_shared_info(shared);

  Handle<Script> script(Script::cast(shared->script()));
  set_script(script);
  if (!script.is_null() && script->type() == Script::TYPE_NATIVE) {
    set_native();
  }
}


ParseInfo::ParseInfo(Zone* zone, Handle<Script> script) : ParseInfo(zone) {
  isolate_ = script->GetIsolate();

  set_hash_seed(isolate_->heap()->HashSeed());
  set_stack_limit(isolate_->stack_guard()->real_climit());
  set_unicode_cache(isolate_->unicode_cache());
  set_script(script);

  if (script->type() == Script::TYPE_NATIVE) {
    set_native();
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
                                            bool call_super, Scope* scope,
                                            int pos, int end_pos,
                                            LanguageMode language_mode) {
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  int parameter_count = 0;
  if (name == nullptr) name = ast_value_factory()->empty_string();

  FunctionKind kind = call_super ? FunctionKind::kDefaultSubclassConstructor
                                 : FunctionKind::kDefaultBaseConstructor;
  Scope* function_scope = NewScope(scope, FUNCTION_SCOPE, kind);
  SetLanguageMode(function_scope,
                  static_cast<LanguageMode>(language_mode | STRICT));
  // Set start and end position to the same value
  function_scope->set_start_position(pos);
  function_scope->set_end_position(pos);
  ZoneList<Statement*>* body = NULL;

  {
    AstNodeFactory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, function_scope,
                                 kind, &function_factory);

    body = new (zone()) ZoneList<Statement*>(call_super ? 2 : 1, zone());
    if (call_super) {
      // $super_constructor = %_GetSuperConstructor(<this-function>)
      // %reflect_construct($super_constructor, arguments, new.target)
      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(2, zone());
      VariableProxy* this_function_proxy = scope_->NewUnresolved(
          factory(), ast_value_factory()->this_function_string(),
          Variable::NORMAL, pos);
      ZoneList<Expression*>* tmp =
          new (zone()) ZoneList<Expression*>(1, zone());
      tmp->Add(this_function_proxy, zone());
      Expression* super_constructor = factory()->NewCallRuntime(
          Runtime::kInlineGetSuperConstructor, tmp, pos);
      args->Add(super_constructor, zone());
      VariableProxy* arguments_proxy = scope_->NewUnresolved(
          factory(), ast_value_factory()->arguments_string(), Variable::NORMAL,
          pos);
      args->Add(arguments_proxy, zone());
      VariableProxy* new_target_proxy = scope_->NewUnresolved(
          factory(), ast_value_factory()->new_target_string(), Variable::NORMAL,
          pos);
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

#define CHECK_OK  ok);   \
  if (!*ok) return NULL; \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

#define CHECK_FAILED  /**/);   \
  if (failed_) return NULL; \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

// ----------------------------------------------------------------------------
// Implementation of Parser

bool ParserTraits::IsEval(const AstRawString* identifier) const {
  return identifier == parser_->ast_value_factory()->eval_string();
}


bool ParserTraits::IsArguments(const AstRawString* identifier) const {
  return identifier == parser_->ast_value_factory()->arguments_string();
}


bool ParserTraits::IsEvalOrArguments(const AstRawString* identifier) const {
  return IsEval(identifier) || IsArguments(identifier);
}

bool ParserTraits::IsUndefined(const AstRawString* identifier) const {
  return identifier == parser_->ast_value_factory()->undefined_string();
}

bool ParserTraits::IsPrototype(const AstRawString* identifier) const {
  return identifier == parser_->ast_value_factory()->prototype_string();
}


bool ParserTraits::IsConstructor(const AstRawString* identifier) const {
  return identifier == parser_->ast_value_factory()->constructor_string();
}


bool ParserTraits::IsThisProperty(Expression* expression) {
  DCHECK(expression != NULL);
  Property* property = expression->AsProperty();
  return property != NULL && property->obj()->IsVariableProxy() &&
         property->obj()->AsVariableProxy()->is_this();
}


bool ParserTraits::IsIdentifier(Expression* expression) {
  VariableProxy* operand = expression->AsVariableProxy();
  return operand != NULL && !operand->is_this();
}


void ParserTraits::PushPropertyName(FuncNameInferrer* fni,
                                    Expression* expression) {
  if (expression->IsPropertyName()) {
    fni->PushLiteralName(expression->AsLiteral()->AsRawPropertyName());
  } else {
    fni->PushLiteralName(
        parser_->ast_value_factory()->anonymous_function_string());
  }
}


void ParserTraits::CheckAssigningFunctionLiteralToProperty(Expression* left,
                                                           Expression* right) {
  DCHECK(left != NULL);
  if (left->IsProperty() && right->IsFunctionLiteral()) {
    right->AsFunctionLiteral()->set_pretenure();
  }
}


Expression* ParserTraits::MarkExpressionAsAssigned(Expression* expression) {
  VariableProxy* proxy =
      expression != NULL ? expression->AsVariableProxy() : NULL;
  if (proxy != NULL) proxy->set_is_assigned();
  return expression;
}


bool ParserTraits::ShortcutNumericLiteralBinaryExpression(
    Expression** x, Expression* y, Token::Value op, int pos,
    AstNodeFactory* factory) {
  if ((*x)->AsLiteral() && (*x)->AsLiteral()->raw_value()->IsNumber() &&
      y->AsLiteral() && y->AsLiteral()->raw_value()->IsNumber()) {
    double x_val = (*x)->AsLiteral()->raw_value()->AsNumber();
    double y_val = y->AsLiteral()->raw_value()->AsNumber();
    bool x_has_dot = (*x)->AsLiteral()->raw_value()->ContainsDot();
    bool y_has_dot = y->AsLiteral()->raw_value()->ContainsDot();
    bool has_dot = x_has_dot || y_has_dot;
    switch (op) {
      case Token::ADD:
        *x = factory->NewNumberLiteral(x_val + y_val, pos, has_dot);
        return true;
      case Token::SUB:
        *x = factory->NewNumberLiteral(x_val - y_val, pos, has_dot);
        return true;
      case Token::MUL:
        *x = factory->NewNumberLiteral(x_val * y_val, pos, has_dot);
        return true;
      case Token::DIV:
        *x = factory->NewNumberLiteral(x_val / y_val, pos, has_dot);
        return true;
      case Token::BIT_OR: {
        int value = DoubleToInt32(x_val) | DoubleToInt32(y_val);
        *x = factory->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::BIT_AND: {
        int value = DoubleToInt32(x_val) & DoubleToInt32(y_val);
        *x = factory->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::BIT_XOR: {
        int value = DoubleToInt32(x_val) ^ DoubleToInt32(y_val);
        *x = factory->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::SHL: {
        int value = DoubleToInt32(x_val) << (DoubleToInt32(y_val) & 0x1f);
        *x = factory->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::SHR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1f;
        uint32_t value = DoubleToUint32(x_val) >> shift;
        *x = factory->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      case Token::SAR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1f;
        int value = ArithmeticShiftRight(DoubleToInt32(x_val), shift);
        *x = factory->NewNumberLiteral(value, pos, has_dot);
        return true;
      }
      default:
        break;
    }
  }
  return false;
}


Expression* ParserTraits::BuildUnaryExpression(Expression* expression,
                                               Token::Value op, int pos,
                                               AstNodeFactory* factory) {
  DCHECK(expression != NULL);
  if (expression->IsLiteral()) {
    const AstValue* literal = expression->AsLiteral()->raw_value();
    if (op == Token::NOT) {
      // Convert the literal to a boolean condition and negate it.
      bool condition = literal->BooleanValue();
      return factory->NewBooleanLiteral(!condition, pos);
    } else if (literal->IsNumber()) {
      // Compute some expressions involving only number literals.
      double value = literal->AsNumber();
      bool has_dot = literal->ContainsDot();
      switch (op) {
        case Token::ADD:
          return expression;
        case Token::SUB:
          return factory->NewNumberLiteral(-value, pos, has_dot);
        case Token::BIT_NOT:
          return factory->NewNumberLiteral(~DoubleToInt32(value), pos, has_dot);
        default:
          break;
      }
    }
  }
  // Desugar '+foo' => 'foo*1'
  if (op == Token::ADD) {
    return factory->NewBinaryOperation(
        Token::MUL, expression, factory->NewNumberLiteral(1, pos, true), pos);
  }
  // The same idea for '-foo' => 'foo*(-1)'.
  if (op == Token::SUB) {
    return factory->NewBinaryOperation(
        Token::MUL, expression, factory->NewNumberLiteral(-1, pos), pos);
  }
  // ...and one more time for '~foo' => 'foo^(~0)'.
  if (op == Token::BIT_NOT) {
    return factory->NewBinaryOperation(
        Token::BIT_XOR, expression, factory->NewNumberLiteral(~0, pos), pos);
  }
  return factory->NewUnaryOperation(op, expression, pos);
}


Expression* ParserTraits::NewThrowReferenceError(
    MessageTemplate::Template message, int pos) {
  return NewThrowError(Runtime::kNewReferenceError, message,
                       parser_->ast_value_factory()->empty_string(), pos);
}


Expression* ParserTraits::NewThrowSyntaxError(MessageTemplate::Template message,
                                              const AstRawString* arg,
                                              int pos) {
  return NewThrowError(Runtime::kNewSyntaxError, message, arg, pos);
}


Expression* ParserTraits::NewThrowTypeError(MessageTemplate::Template message,
                                            const AstRawString* arg, int pos) {
  return NewThrowError(Runtime::kNewTypeError, message, arg, pos);
}


Expression* ParserTraits::NewThrowError(Runtime::FunctionId id,
                                        MessageTemplate::Template message,
                                        const AstRawString* arg, int pos) {
  Zone* zone = parser_->zone();
  ZoneList<Expression*>* args = new (zone) ZoneList<Expression*>(2, zone);
  args->Add(parser_->factory()->NewSmiLiteral(message, pos), zone);
  args->Add(parser_->factory()->NewStringLiteral(arg, pos), zone);
  CallRuntime* call_constructor =
      parser_->factory()->NewCallRuntime(id, args, pos);
  return parser_->factory()->NewThrow(call_constructor, pos);
}


void ParserTraits::ReportMessageAt(Scanner::Location source_location,
                                   MessageTemplate::Template message,
                                   const char* arg, ParseErrorType error_type) {
  if (parser_->stack_overflow()) {
    // Suppress the error message (syntax error or such) in the presence of a
    // stack overflow. The isolate allows only one pending exception at at time
    // and we want to report the stack overflow later.
    return;
  }
  parser_->pending_error_handler_.ReportMessageAt(source_location.beg_pos,
                                                  source_location.end_pos,
                                                  message, arg, error_type);
}


void ParserTraits::ReportMessage(MessageTemplate::Template message,
                                 const char* arg, ParseErrorType error_type) {
  Scanner::Location source_location = parser_->scanner()->location();
  ReportMessageAt(source_location, message, arg, error_type);
}


void ParserTraits::ReportMessage(MessageTemplate::Template message,
                                 const AstRawString* arg,
                                 ParseErrorType error_type) {
  Scanner::Location source_location = parser_->scanner()->location();
  ReportMessageAt(source_location, message, arg, error_type);
}


void ParserTraits::ReportMessageAt(Scanner::Location source_location,
                                   MessageTemplate::Template message,
                                   const AstRawString* arg,
                                   ParseErrorType error_type) {
  if (parser_->stack_overflow()) {
    // Suppress the error message (syntax error or such) in the presence of a
    // stack overflow. The isolate allows only one pending exception at at time
    // and we want to report the stack overflow later.
    return;
  }
  parser_->pending_error_handler_.ReportMessageAt(source_location.beg_pos,
                                                  source_location.end_pos,
                                                  message, arg, error_type);
}


const AstRawString* ParserTraits::GetSymbol(Scanner* scanner) {
  const AstRawString* result =
      parser_->scanner()->CurrentSymbol(parser_->ast_value_factory());
  DCHECK(result != NULL);
  return result;
}


const AstRawString* ParserTraits::GetNumberAsSymbol(Scanner* scanner) {
  double double_value = parser_->scanner()->DoubleValue();
  char array[100];
  const char* string =
      DoubleToCString(double_value, Vector<char>(array, arraysize(array)));
  return parser_->ast_value_factory()->GetOneByteString(string);
}


const AstRawString* ParserTraits::GetNextSymbol(Scanner* scanner) {
  return parser_->scanner()->NextSymbol(parser_->ast_value_factory());
}


Expression* ParserTraits::ThisExpression(Scope* scope, AstNodeFactory* factory,
                                         int pos) {
  return scope->NewUnresolved(factory,
                              parser_->ast_value_factory()->this_string(),
                              Variable::THIS, pos, pos + 4);
}


Expression* ParserTraits::SuperPropertyReference(Scope* scope,
                                                 AstNodeFactory* factory,
                                                 int pos) {
  // this_function[home_object_symbol]
  VariableProxy* this_function_proxy = scope->NewUnresolved(
      factory, parser_->ast_value_factory()->this_function_string(),
      Variable::NORMAL, pos);
  Expression* home_object_symbol_literal =
      factory->NewSymbolLiteral("home_object_symbol", RelocInfo::kNoPosition);
  Expression* home_object = factory->NewProperty(
      this_function_proxy, home_object_symbol_literal, pos);
  return factory->NewSuperPropertyReference(
      ThisExpression(scope, factory, pos)->AsVariableProxy(), home_object, pos);
}


Expression* ParserTraits::SuperCallReference(Scope* scope,
                                             AstNodeFactory* factory, int pos) {
  VariableProxy* new_target_proxy = scope->NewUnresolved(
      factory, parser_->ast_value_factory()->new_target_string(),
      Variable::NORMAL, pos);
  VariableProxy* this_function_proxy = scope->NewUnresolved(
      factory, parser_->ast_value_factory()->this_function_string(),
      Variable::NORMAL, pos);
  return factory->NewSuperCallReference(
      ThisExpression(scope, factory, pos)->AsVariableProxy(), new_target_proxy,
      this_function_proxy, pos);
}


Expression* ParserTraits::NewTargetExpression(Scope* scope,
                                              AstNodeFactory* factory,
                                              int pos) {
  static const int kNewTargetStringLength = 10;
  auto proxy = scope->NewUnresolved(
      factory, parser_->ast_value_factory()->new_target_string(),
      Variable::NORMAL, pos, pos + kNewTargetStringLength);
  proxy->set_is_new_target();
  return proxy;
}


Expression* ParserTraits::FunctionSentExpression(Scope* scope,
                                                 AstNodeFactory* factory,
                                                 int pos) {
  // We desugar function.sent into %GeneratorGetInput(generator).
  Zone* zone = parser_->zone();
  ZoneList<Expression*>* args = new (zone) ZoneList<Expression*>(1, zone);
  VariableProxy* generator = factory->NewVariableProxy(
      parser_->function_state_->generator_object_variable());
  args->Add(generator, zone);
  return factory->NewCallRuntime(Runtime::kGeneratorGetInput, args, pos);
}


Literal* ParserTraits::ExpressionFromLiteral(Token::Value token, int pos,
                                             Scanner* scanner,
                                             AstNodeFactory* factory) {
  switch (token) {
    case Token::NULL_LITERAL:
      return factory->NewNullLiteral(pos);
    case Token::TRUE_LITERAL:
      return factory->NewBooleanLiteral(true, pos);
    case Token::FALSE_LITERAL:
      return factory->NewBooleanLiteral(false, pos);
    case Token::SMI: {
      int value = scanner->smi_value();
      return factory->NewSmiLiteral(value, pos);
    }
    case Token::NUMBER: {
      bool has_dot = scanner->ContainsDot();
      double value = scanner->DoubleValue();
      return factory->NewNumberLiteral(value, pos, has_dot);
    }
    default:
      DCHECK(false);
  }
  return NULL;
}


Expression* ParserTraits::ExpressionFromIdentifier(const AstRawString* name,
                                                   int start_position,
                                                   int end_position,
                                                   Scope* scope,
                                                   AstNodeFactory* factory) {
  if (parser_->fni_ != NULL) parser_->fni_->PushVariableName(name);
  return scope->NewUnresolved(factory, name, Variable::NORMAL, start_position,
                              end_position);
}


Expression* ParserTraits::ExpressionFromString(int pos, Scanner* scanner,
                                               AstNodeFactory* factory) {
  const AstRawString* symbol = GetSymbol(scanner);
  if (parser_->fni_ != NULL) parser_->fni_->PushLiteralName(symbol);
  return factory->NewStringLiteral(symbol, pos);
}


Expression* ParserTraits::GetIterator(Expression* iterable,
                                      AstNodeFactory* factory, int pos) {
  Expression* iterator_symbol_literal =
      factory->NewSymbolLiteral("iterator_symbol", RelocInfo::kNoPosition);
  Expression* prop =
      factory->NewProperty(iterable, iterator_symbol_literal, pos);
  Zone* zone = parser_->zone();
  ZoneList<Expression*>* args = new (zone) ZoneList<Expression*>(0, zone);
  return factory->NewCall(prop, args, pos);
}


Literal* ParserTraits::GetLiteralTheHole(int position,
                                         AstNodeFactory* factory) {
  return factory->NewTheHoleLiteral(RelocInfo::kNoPosition);
}


Expression* ParserTraits::ParseV8Intrinsic(bool* ok) {
  return parser_->ParseV8Intrinsic(ok);
}


FunctionLiteral* ParserTraits::ParseFunctionLiteral(
    const AstRawString* name, Scanner::Location function_name_location,
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_position, FunctionLiteral::FunctionType type,
    LanguageMode language_mode, bool* ok) {
  return parser_->ParseFunctionLiteral(
      name, function_name_location, function_name_validity, kind,
      function_token_position, type, language_mode, ok);
}


ClassLiteral* ParserTraits::ParseClassLiteral(
    const AstRawString* name, Scanner::Location class_name_location,
    bool name_is_strict_reserved, int pos, bool* ok) {
  return parser_->ParseClassLiteral(name, class_name_location,
                                    name_is_strict_reserved, pos, ok);
}


Parser::Parser(ParseInfo* info)
    : ParserBase<ParserTraits>(info->zone(), &scanner_, info->stack_limit(),
                               info->extension(), info->ast_value_factory(),
                               NULL, this),
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
  DCHECK(!info->script().is_null() || info->source_stream() != NULL);
  set_allow_lazy(info->allow_lazy_parsing());
  set_allow_natives(FLAG_allow_natives_syntax || info->is_native());
  set_allow_harmony_sloppy(FLAG_harmony_sloppy);
  set_allow_harmony_sloppy_function(FLAG_harmony_sloppy_function);
  set_allow_harmony_sloppy_let(FLAG_harmony_sloppy_let);
  set_allow_harmony_default_parameters(FLAG_harmony_default_parameters);
  set_allow_harmony_destructuring_bind(FLAG_harmony_destructuring_bind);
  set_allow_harmony_destructuring_assignment(
      FLAG_harmony_destructuring_assignment);
  set_allow_strong_mode(FLAG_strong_mode);
  set_allow_legacy_const(FLAG_legacy_const);
  set_allow_harmony_do_expressions(FLAG_harmony_do_expressions);
  set_allow_harmony_function_name(FLAG_harmony_function_name);
  set_allow_harmony_function_sent(FLAG_harmony_function_sent);
  for (int feature = 0; feature < v8::Isolate::kUseCounterFeatureCount;
       ++feature) {
    use_counts_[feature] = 0;
  }
  if (info->ast_value_factory() == NULL) {
    // info takes ownership of AstValueFactory.
    info->set_ast_value_factory(new AstValueFactory(zone(), info->hash_seed()));
    info->set_ast_value_factory_owned();
    ast_value_factory_ = info->ast_value_factory();
  }
}


FunctionLiteral* Parser::ParseProgram(Isolate* isolate, ParseInfo* info) {
  // TODO(bmeurer): We temporarily need to pass allow_nesting = true here,
  // see comment for HistogramTimerScope class.

  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);

  HistogramTimerScope timer_scope(isolate->counters()->parse(), true);
  TRACE_EVENT0("v8", "V8.Parse");
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

  source = String::Flatten(source);
  FunctionLiteral* result;

  if (source->IsExternalTwoByteString()) {
    // Notice that the stream is destroyed at the end of the branch block.
    // The last line of the blocks can't be moved outside, even though they're
    // identical calls.
    ExternalTwoByteStringUtf16CharacterStream stream(
        Handle<ExternalTwoByteString>::cast(source), 0, source->length());
    scanner_.Initialize(&stream);
    result = DoParseProgram(info);
  } else {
    GenericStringUtf16CharacterStream stream(source, 0, source->length());
    scanner_.Initialize(&stream);
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
      base::SmartArrayPointer<char> name_chars = name->ToCString();
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
  DCHECK(scope_ == NULL);
  DCHECK(target_stack_ == NULL);

  Mode parsing_mode = FLAG_lazy && allow_lazy() ? PARSE_LAZILY : PARSE_EAGERLY;
  if (allow_natives() || extension_ != NULL) parsing_mode = PARSE_EAGERLY;

  FunctionLiteral* result = NULL;
  {
    // TODO(wingo): Add an outer SCRIPT_SCOPE corresponding to the native
    // context, which will have the "this" binding for script scopes.
    Scope* scope = NewScope(scope_, SCRIPT_SCOPE);
    info->set_script_scope(scope);
    if (!info->context().is_null() && !info->context()->IsNativeContext()) {
      scope = Scope::DeserializeScopeChain(info->isolate(), zone(),
                                           *info->context(), scope);
      // The Scope is backed up by ScopeInfo (which is in the V8 heap); this
      // means the Parser cannot operate independent of the V8 heap. Tell the
      // string table to internalize strings and values right after they're
      // created. This kind of parsing can only be done in the main thread.
      DCHECK(parsing_on_main_thread_);
      ast_value_factory()->Internalize(info->isolate());
    }
    original_scope_ = scope;
    if (info->is_eval()) {
      if (!scope->is_script_scope() || is_strict(info->language_mode())) {
        parsing_mode = PARSE_EAGERLY;
      }
      scope = NewScope(scope, EVAL_SCOPE);
    } else if (info->is_module()) {
      scope = NewScope(scope, MODULE_SCOPE);
    }

    scope->set_start_position(0);

    // Enter 'scope' with the given parsing mode.
    ParsingModeScope parsing_mode_scope(this, parsing_mode);
    AstNodeFactory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, scope,
                                 kNormalFunction, &function_factory);

    // Don't count the mode in the use counters--give the program a chance
    // to enable script/module-wide strict/strong mode below.
    scope_->SetLanguageMode(info->language_mode());
    ZoneList<Statement*>* body = new(zone()) ZoneList<Statement*>(16, zone());
    bool ok = true;
    int beg_pos = scanner()->location().beg_pos;
    if (info->is_module()) {
      ParseModuleItemList(body, &ok);
    } else {
      ParseStatementList(body, Token::EOS, &ok);
    }

    // The parser will peek but not consume EOS.  Our scope logically goes all
    // the way to the EOS, though.
    scope->set_end_position(scanner()->peek_location().beg_pos);

    if (ok && is_strict(language_mode())) {
      CheckStrictOctalLiteral(beg_pos, scanner()->location().end_pos, &ok);
    }
    if (ok && is_sloppy(language_mode()) && allow_harmony_sloppy_function()) {
      // TODO(littledan): Function bindings on the global object that modify
      // pre-existing bindings should be made writable, enumerable and
      // nonconfigurable if possible, whereas this code will leave attributes
      // unchanged if the property already exists.
      InsertSloppyBlockFunctionVarBindings(scope, &ok);
    }
    if (ok && (is_strict(language_mode()) || allow_harmony_sloppy() ||
               allow_harmony_destructuring_bind())) {
      CheckConflictingVarDeclarations(scope_, &ok);
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
      ParserTraits::RewriteDestructuringAssignments();
      result = factory()->NewScriptOrEvalFunctionLiteral(
          scope_, body, function_state.materialized_literal_count(),
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
  HistogramTimerScope timer_scope(isolate->counters()->parse_lazy());
  TRACE_EVENT0("v8", "V8.ParseLazy");
  Handle<String> source(String::cast(info->script()->source()));
  isolate->counters()->total_parse_size()->Increment(source->length());
  base::ElapsedTimer timer;
  if (FLAG_trace_parse) {
    timer.Start();
  }
  Handle<SharedFunctionInfo> shared_info = info->shared_info();

  // Initialize parser state.
  source = String::Flatten(source);
  FunctionLiteral* result;
  if (source->IsExternalTwoByteString()) {
    ExternalTwoByteStringUtf16CharacterStream stream(
        Handle<ExternalTwoByteString>::cast(source),
        shared_info->start_position(),
        shared_info->end_position());
    result = ParseLazy(isolate, info, &stream);
  } else {
    GenericStringUtf16CharacterStream stream(source,
                                             shared_info->start_position(),
                                             shared_info->end_position());
    result = ParseLazy(isolate, info, &stream);
  }

  if (FLAG_trace_parse && result != NULL) {
    double ms = timer.Elapsed().InMillisecondsF();
    base::SmartArrayPointer<char> name_chars =
        result->debug_name()->ToCString();
    PrintF("[parsing function: %s - took %0.3f ms]\n", name_chars.get(), ms);
  }
  return result;
}

static FunctionLiteral::FunctionType ComputeFunctionType(
    Handle<SharedFunctionInfo> shared_info) {
  if (shared_info->is_declaration()) {
    return FunctionLiteral::kDeclaration;
  } else if (shared_info->is_named_expression()) {
    return FunctionLiteral::kNamedExpression;
  } else if (IsConciseMethod(shared_info->kind()) ||
             IsAccessorFunction(shared_info->kind())) {
    return FunctionLiteral::kAccessorOrMethod;
  }
  return FunctionLiteral::kAnonymousExpression;
}

FunctionLiteral* Parser::ParseLazy(Isolate* isolate, ParseInfo* info,
                                   Utf16CharacterStream* source) {
  Handle<SharedFunctionInfo> shared_info = info->shared_info();
  scanner_.Initialize(source);
  DCHECK(scope_ == NULL);
  DCHECK(target_stack_ == NULL);

  Handle<String> name(String::cast(shared_info->name()));
  DCHECK(ast_value_factory());
  fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());
  const AstRawString* raw_name = ast_value_factory()->GetString(name);
  fni_->PushEnclosingName(raw_name);

  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  // Place holder for the result.
  FunctionLiteral* result = NULL;

  {
    // Parse the function literal.
    Scope* scope = NewScope(scope_, SCRIPT_SCOPE);
    info->set_script_scope(scope);
    if (!info->closure().is_null()) {
      // Ok to use Isolate here, since lazy function parsing is only done in the
      // main thread.
      DCHECK(parsing_on_main_thread_);
      scope = Scope::DeserializeScopeChain(isolate, zone(),
                                           info->closure()->context(), scope);
    }
    original_scope_ = scope;
    AstNodeFactory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, scope,
                                 shared_info->kind(), &function_factory);
    DCHECK(is_sloppy(scope->language_mode()) ||
           is_strict(info->language_mode()));
    DCHECK(info->language_mode() == shared_info->language_mode());
    FunctionLiteral::FunctionType function_type =
        ComputeFunctionType(shared_info);
    bool ok = true;

    if (shared_info->is_arrow()) {
      // TODO(adamk): We should construct this scope from the ScopeInfo.
      Scope* scope =
          NewScope(scope_, FUNCTION_SCOPE, FunctionKind::kArrowFunction);

      // These two bits only need to be explicitly set because we're
      // not passing the ScopeInfo to the Scope constructor.
      // TODO(adamk): Remove these calls once the above NewScope call
      // passes the ScopeInfo.
      if (shared_info->scope_info()->CallsEval()) {
        scope->RecordEvalCall();
      }
      SetLanguageMode(scope, shared_info->language_mode());

      scope->set_start_position(shared_info->start_position());
      ExpressionClassifier formals_classifier(this);
      ParserFormalParameters formals(scope);
      Checkpoint checkpoint(this);
      {
        // Parsing patterns as variable reference expression creates
        // NewUnresolved references in current scope. Entrer arrow function
        // scope for formal parameter parsing.
        BlockState block_state(&scope_, scope);
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
        Expression* expression =
            ParseArrowFunctionLiteral(true, formals, formals_classifier, &ok);
        if (ok) {
          // Scanning must end at the same position that was recorded
          // previously. If not, parsing has been interrupted due to a stack
          // overflow, at which point the partially parsed arrow function
          // concise body happens to be a valid expression. This is a problem
          // only for arrow functions with single expression bodies, since there
          // is no end token such as "}" for normal functions.
          if (scanner()->location().end_pos == shared_info->end_position()) {
            // The pre-parser saw an arrow function here, so the full parser
            // must produce a FunctionLiteral.
            DCHECK(expression->IsFunctionLiteral());
            result = expression->AsFunctionLiteral();
          } else {
            ok = false;
          }
        }
      }
    } else if (shared_info->is_default_constructor()) {
      result = DefaultConstructor(
          raw_name, IsSubclassConstructor(shared_info->kind()), scope,
          shared_info->start_position(), shared_info->end_position(),
          shared_info->language_mode());
    } else {
      result = ParseFunctionLiteral(raw_name, Scanner::Location::invalid(),
                                    kSkipFunctionNameCheck, shared_info->kind(),
                                    RelocInfo::kNoPosition, function_type,
                                    shared_info->language_mode(), &ok);
    }
    // Make sure the results agree.
    DCHECK(ok == (result != NULL));
  }

  // Make sure the target stack is empty.
  DCHECK(target_stack_ == NULL);

  if (result != NULL) {
    Handle<String> inferred_name(shared_info->inferred_name());
    result->set_inferred_name(inferred_name);
  }
  return result;
}


void* Parser::ParseStatementList(ZoneList<Statement*>* body, int end_token,
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
    Scanner::Location old_this_loc = function_state_->this_location();
    Scanner::Location old_super_loc = function_state_->super_location();
    Statement* stat = ParseStatementListItem(CHECK_OK);

    if (is_strong(language_mode()) && scope_->is_function_scope() &&
        IsClassConstructor(function_state_->kind())) {
      Scanner::Location this_loc = function_state_->this_location();
      Scanner::Location super_loc = function_state_->super_location();
      if (this_loc.beg_pos != old_this_loc.beg_pos &&
          this_loc.beg_pos != token_loc.beg_pos) {
        ReportMessageAt(this_loc, MessageTemplate::kStrongConstructorThis);
        *ok = false;
        return nullptr;
      }
      if (super_loc.beg_pos != old_super_loc.beg_pos &&
          super_loc.beg_pos != token_loc.beg_pos) {
        ReportMessageAt(super_loc, MessageTemplate::kStrongConstructorSuper);
        *ok = false;
        return nullptr;
      }
    }

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
        // Check "use strict" directive (ES5 14.1), "use asm" directive, and
        // "use strong" directive (experimental).
        bool use_strict_found =
            literal->raw_value()->AsString() ==
                ast_value_factory()->use_strict_string() &&
            token_loc.end_pos - token_loc.beg_pos ==
                ast_value_factory()->use_strict_string()->length() + 2;
        bool use_strong_found =
            allow_strong_mode() &&
            literal->raw_value()->AsString() ==
                ast_value_factory()->use_strong_string() &&
            token_loc.end_pos - token_loc.beg_pos ==
                ast_value_factory()->use_strong_string()->length() + 2;
        if (use_strict_found || use_strong_found) {
          // Strong mode implies strict mode. If there are several "use strict"
          // / "use strong" directives, do the strict mode changes only once.
          if (is_sloppy(scope_->language_mode())) {
            RaiseLanguageMode(STRICT);
          }

          if (use_strong_found) {
            RaiseLanguageMode(STRONG);
            if (IsClassConstructor(function_state_->kind())) {
              // "use strong" cannot occur in a class constructor body, to avoid
              // unintuitive strong class object semantics.
              ParserTraits::ReportMessageAt(
                  token_loc, MessageTemplate::kStrongConstructorDirective);
              *ok = false;
              return nullptr;
            }
          }
          if (!scope_->HasSimpleParameters()) {
            // TC39 deemed "use strict" directives to be an error when occurring
            // in the body of a function with non-simple parameter list, on
            // 29/7/2015. https://goo.gl/ueA7Ln
            //
            // In V8, this also applies to "use strong " directives.
            const AstRawString* string = literal->raw_value()->AsString();
            ParserTraits::ReportMessageAt(
                token_loc, MessageTemplate::kIllegalLanguageModeDirective,
                string);
            *ok = false;
            return nullptr;
          }
          // Because declarations in strict eval code don't leak into the scope
          // of the eval call, it is likely that functions declared in strict
          // eval code will be used within the eval code, so lazy parsing is
          // probably not a win.
          if (scope_->is_eval_scope()) mode_ = PARSE_EAGERLY;
        } else if (literal->raw_value()->AsString() ==
                       ast_value_factory()->use_asm_string() &&
                   token_loc.end_pos - token_loc.beg_pos ==
                       ast_value_factory()->use_asm_string()->length() + 2) {
          // Store the usage count; The actual use counter on the isolate is
          // incremented after parsing is done.
          ++use_counts_[v8::Isolate::kUseAsm];
          scope_->SetAsmModule();
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

  return 0;
}


Statement* Parser::ParseStatementListItem(bool* ok) {
  // (Ecma 262 6th Edition, 13.1):
  // StatementListItem:
  //    Statement
  //    Declaration

  switch (peek()) {
    case Token::FUNCTION:
      return ParseFunctionDeclaration(NULL, ok);
    case Token::CLASS:
      Consume(Token::CLASS);
      return ParseClassDeclaration(NULL, ok);
    case Token::CONST:
      if (allow_const()) {
        return ParseVariableStatement(kStatementListItem, NULL, ok);
      }
      break;
    case Token::VAR:
      return ParseVariableStatement(kStatementListItem, NULL, ok);
    case Token::LET:
      if (IsNextLetKeyword()) {
        return ParseVariableStatement(kStatementListItem, NULL, ok);
      }
      break;
    default:
      break;
  }
  return ParseStatement(NULL, ok);
}


Statement* Parser::ParseModuleItem(bool* ok) {
  // (Ecma 262 6th Edition, 15.2):
  // ModuleItem :
  //    ImportDeclaration
  //    ExportDeclaration
  //    StatementListItem

  switch (peek()) {
    case Token::IMPORT:
      return ParseImportDeclaration(ok);
    case Token::EXPORT:
      return ParseExportDeclaration(ok);
    default:
      return ParseStatementListItem(ok);
  }
}


void* Parser::ParseModuleItemList(ZoneList<Statement*>* body, bool* ok) {
  // (Ecma 262 6th Edition, 15.2):
  // Module :
  //    ModuleBody?
  //
  // ModuleBody :
  //    ModuleItem*

  DCHECK(scope_->is_module_scope());
  RaiseLanguageMode(STRICT);

  while (peek() != Token::EOS) {
    Statement* stat = ParseModuleItem(CHECK_OK);
    if (stat && !stat->IsEmpty()) {
      body->Add(stat, zone());
    }
  }

  // Check that all exports are bound.
  ModuleDescriptor* descriptor = scope_->module();
  for (ModuleDescriptor::Iterator it = descriptor->iterator(); !it.done();
       it.Advance()) {
    if (scope_->LookupLocal(it.local_name()) == NULL) {
      // TODO(adamk): Pass both local_name and export_name once ParserTraits
      // supports multiple arg error messages.
      // Also try to report this at a better location.
      ParserTraits::ReportMessage(MessageTemplate::kModuleExportUndefined,
                                  it.local_name());
      *ok = false;
      return NULL;
    }
  }

  return NULL;
}


const AstRawString* Parser::ParseModuleSpecifier(bool* ok) {
  // ModuleSpecifier :
  //    StringLiteral

  Expect(Token::STRING, CHECK_OK);
  return GetSymbol(scanner());
}


void* Parser::ParseExportClause(ZoneList<const AstRawString*>* export_names,
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

  Expect(Token::LBRACE, CHECK_OK);

  Token::Value name_tok;
  while ((name_tok = peek()) != Token::RBRACE) {
    // Keep track of the first reserved word encountered in case our
    // caller needs to report an error.
    if (!reserved_loc->IsValid() &&
        !Token::IsIdentifier(name_tok, STRICT, false)) {
      *reserved_loc = scanner()->location();
    }
    const AstRawString* local_name = ParseIdentifierName(CHECK_OK);
    const AstRawString* export_name = NULL;
    if (CheckContextualKeyword(CStrVector("as"))) {
      export_name = ParseIdentifierName(CHECK_OK);
    }
    if (export_name == NULL) {
      export_name = local_name;
    }
    export_names->Add(export_name, zone());
    local_names->Add(local_name, zone());
    export_locations->Add(scanner()->location(), zone());
    if (peek() == Token::RBRACE) break;
    Expect(Token::COMMA, CHECK_OK);
  }

  Expect(Token::RBRACE, CHECK_OK);

  return 0;
}


ZoneList<ImportDeclaration*>* Parser::ParseNamedImports(int pos, bool* ok) {
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

  ZoneList<ImportDeclaration*>* result =
      new (zone()) ZoneList<ImportDeclaration*>(1, zone());
  while (peek() != Token::RBRACE) {
    const AstRawString* import_name = ParseIdentifierName(CHECK_OK);
    const AstRawString* local_name = import_name;
    // In the presence of 'as', the left-side of the 'as' can
    // be any IdentifierName. But without 'as', it must be a valid
    // BindingIdentifier.
    if (CheckContextualKeyword(CStrVector("as"))) {
      local_name = ParseIdentifierName(CHECK_OK);
    }
    if (!Token::IsIdentifier(scanner()->current_token(), STRICT, false)) {
      *ok = false;
      ReportMessage(MessageTemplate::kUnexpectedReserved);
      return NULL;
    } else if (IsEvalOrArguments(local_name)) {
      *ok = false;
      ReportMessage(MessageTemplate::kStrictEvalArguments);
      return NULL;
    } else if (is_strong(language_mode()) && IsUndefined(local_name)) {
      *ok = false;
      ReportMessage(MessageTemplate::kStrongUndefined);
      return NULL;
    }
    VariableProxy* proxy = NewUnresolved(local_name, IMPORT);
    ImportDeclaration* declaration =
        factory()->NewImportDeclaration(proxy, import_name, NULL, scope_, pos);
    Declare(declaration, DeclarationDescriptor::NORMAL, true, CHECK_OK);
    result->Add(declaration, zone());
    if (peek() == Token::RBRACE) break;
    Expect(Token::COMMA, CHECK_OK);
  }

  Expect(Token::RBRACE, CHECK_OK);

  return result;
}


Statement* Parser::ParseImportDeclaration(bool* ok) {
  // ImportDeclaration :
  //   'import' ImportClause 'from' ModuleSpecifier ';'
  //   'import' ModuleSpecifier ';'
  //
  // ImportClause :
  //   NameSpaceImport
  //   NamedImports
  //   ImportedDefaultBinding
  //   ImportedDefaultBinding ',' NameSpaceImport
  //   ImportedDefaultBinding ',' NamedImports
  //
  // NameSpaceImport :
  //   '*' 'as' ImportedBinding

  int pos = peek_position();
  Expect(Token::IMPORT, CHECK_OK);

  Token::Value tok = peek();

  // 'import' ModuleSpecifier ';'
  if (tok == Token::STRING) {
    const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK);
    scope_->module()->AddModuleRequest(module_specifier, zone());
    ExpectSemicolon(CHECK_OK);
    return factory()->NewEmptyStatement(pos);
  }

  // Parse ImportedDefaultBinding if present.
  ImportDeclaration* import_default_declaration = NULL;
  if (tok != Token::MUL && tok != Token::LBRACE) {
    const AstRawString* local_name =
        ParseIdentifier(kDontAllowRestrictedIdentifiers, CHECK_OK);
    VariableProxy* proxy = NewUnresolved(local_name, IMPORT);
    import_default_declaration = factory()->NewImportDeclaration(
        proxy, ast_value_factory()->default_string(), NULL, scope_, pos);
    Declare(import_default_declaration, DeclarationDescriptor::NORMAL, true,
            CHECK_OK);
  }

  const AstRawString* module_instance_binding = NULL;
  ZoneList<ImportDeclaration*>* named_declarations = NULL;
  if (import_default_declaration == NULL || Check(Token::COMMA)) {
    switch (peek()) {
      case Token::MUL: {
        Consume(Token::MUL);
        ExpectContextualKeyword(CStrVector("as"), CHECK_OK);
        module_instance_binding =
            ParseIdentifier(kDontAllowRestrictedIdentifiers, CHECK_OK);
        // TODO(ES6): Add an appropriate declaration.
        break;
      }

      case Token::LBRACE:
        named_declarations = ParseNamedImports(pos, CHECK_OK);
        break;

      default:
        *ok = false;
        ReportUnexpectedToken(scanner()->current_token());
        return NULL;
    }
  }

  ExpectContextualKeyword(CStrVector("from"), CHECK_OK);
  const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK);
  scope_->module()->AddModuleRequest(module_specifier, zone());

  if (module_instance_binding != NULL) {
    // TODO(ES6): Set the module specifier for the module namespace binding.
  }

  if (import_default_declaration != NULL) {
    import_default_declaration->set_module_specifier(module_specifier);
  }

  if (named_declarations != NULL) {
    for (int i = 0; i < named_declarations->length(); ++i) {
      named_declarations->at(i)->set_module_specifier(module_specifier);
    }
  }

  ExpectSemicolon(CHECK_OK);
  return factory()->NewEmptyStatement(pos);
}


Statement* Parser::ParseExportDefault(bool* ok) {
  //  Supports the following productions, starting after the 'default' token:
  //    'export' 'default' FunctionDeclaration
  //    'export' 'default' ClassDeclaration
  //    'export' 'default' AssignmentExpression[In] ';'

  Expect(Token::DEFAULT, CHECK_OK);
  Scanner::Location default_loc = scanner()->location();

  const AstRawString* default_string = ast_value_factory()->default_string();
  ZoneList<const AstRawString*> names(1, zone());
  Statement* result = nullptr;
  Expression* default_export = nullptr;
  switch (peek()) {
    case Token::FUNCTION: {
      Consume(Token::FUNCTION);
      int pos = position();
      bool is_generator = Check(Token::MUL);
      if (peek() == Token::LPAREN) {
        // FunctionDeclaration[+Default] ::
        //   'function' '(' FormalParameters ')' '{' FunctionBody '}'
        //
        // GeneratorDeclaration[+Default] ::
        //   'function' '*' '(' FormalParameters ')' '{' FunctionBody '}'
        default_export = ParseFunctionLiteral(
            default_string, Scanner::Location::invalid(),
            kSkipFunctionNameCheck,
            is_generator ? FunctionKind::kGeneratorFunction
                         : FunctionKind::kNormalFunction,
            pos, FunctionLiteral::kDeclaration, language_mode(), CHECK_OK);
        result = factory()->NewEmptyStatement(RelocInfo::kNoPosition);
      } else {
        result = ParseFunctionDeclaration(pos, is_generator, &names, CHECK_OK);
      }
      break;
    }

    case Token::CLASS:
      Consume(Token::CLASS);
      if (peek() == Token::EXTENDS || peek() == Token::LBRACE) {
        // ClassDeclaration[+Default] ::
        //   'class' ('extends' LeftHandExpression)? '{' ClassBody '}'
        default_export =
            ParseClassLiteral(default_string, Scanner::Location::invalid(),
                              false, position(), CHECK_OK);
        result = factory()->NewEmptyStatement(RelocInfo::kNoPosition);
      } else {
        result = ParseClassDeclaration(&names, CHECK_OK);
      }
      break;

    default: {
      int pos = peek_position();
      ExpressionClassifier classifier(this);
      Expression* expr = ParseAssignmentExpression(true, &classifier, CHECK_OK);
      RewriteNonPattern(&classifier, CHECK_OK);

      ExpectSemicolon(CHECK_OK);
      result = factory()->NewExpressionStatement(expr, pos);
      break;
    }
  }

  DCHECK_LE(names.length(), 1);
  if (names.length() == 1) {
    scope_->module()->AddLocalExport(default_string, names.first(), zone(), ok);
    if (!*ok) {
      ParserTraits::ReportMessageAt(
          default_loc, MessageTemplate::kDuplicateExport, default_string);
      return nullptr;
    }
  } else {
    // TODO(ES6): Assign result to a const binding with the name "*default*"
    // and add an export entry with "*default*" as the local name.
    USE(default_export);
  }

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

  Statement* result = NULL;
  ZoneList<const AstRawString*> names(1, zone());
  switch (peek()) {
    case Token::DEFAULT:
      return ParseExportDefault(ok);

    case Token::MUL: {
      Consume(Token::MUL);
      ExpectContextualKeyword(CStrVector("from"), CHECK_OK);
      const AstRawString* module_specifier = ParseModuleSpecifier(CHECK_OK);
      scope_->module()->AddModuleRequest(module_specifier, zone());
      // TODO(ES6): scope_->module()->AddStarExport(...)
      ExpectSemicolon(CHECK_OK);
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
      ZoneList<const AstRawString*> local_names(1, zone());
      ParseExportClause(&export_names, &export_locations, &local_names,
                        &reserved_loc, CHECK_OK);
      const AstRawString* indirect_export_module_specifier = NULL;
      if (CheckContextualKeyword(CStrVector("from"))) {
        indirect_export_module_specifier = ParseModuleSpecifier(CHECK_OK);
      } else if (reserved_loc.IsValid()) {
        // No FromClause, so reserved words are invalid in ExportClause.
        *ok = false;
        ReportMessageAt(reserved_loc, MessageTemplate::kUnexpectedReserved);
        return NULL;
      }
      ExpectSemicolon(CHECK_OK);
      const int length = export_names.length();
      DCHECK_EQ(length, local_names.length());
      DCHECK_EQ(length, export_locations.length());
      if (indirect_export_module_specifier == NULL) {
        for (int i = 0; i < length; ++i) {
          scope_->module()->AddLocalExport(export_names[i], local_names[i],
                                           zone(), ok);
          if (!*ok) {
            ParserTraits::ReportMessageAt(export_locations[i],
                                          MessageTemplate::kDuplicateExport,
                                          export_names[i]);
            return NULL;
          }
        }
      } else {
        scope_->module()->AddModuleRequest(indirect_export_module_specifier,
                                           zone());
        for (int i = 0; i < length; ++i) {
          // TODO(ES6): scope_->module()->AddIndirectExport(...);(
        }
      }
      return factory()->NewEmptyStatement(pos);
    }

    case Token::FUNCTION:
      result = ParseFunctionDeclaration(&names, CHECK_OK);
      break;

    case Token::CLASS:
      Consume(Token::CLASS);
      result = ParseClassDeclaration(&names, CHECK_OK);
      break;

    case Token::VAR:
    case Token::LET:
    case Token::CONST:
      result = ParseVariableStatement(kStatementListItem, &names, CHECK_OK);
      break;

    default:
      *ok = false;
      ReportUnexpectedToken(scanner()->current_token());
      return NULL;
  }

  // Extract declared names into export declarations.
  ModuleDescriptor* descriptor = scope_->module();
  for (int i = 0; i < names.length(); ++i) {
    descriptor->AddLocalExport(names[i], names[i], zone(), ok);
    if (!*ok) {
      // TODO(adamk): Possibly report this error at the right place.
      ParserTraits::ReportMessage(MessageTemplate::kDuplicateExport, names[i]);
      return NULL;
    }
  }

  DCHECK_NOT_NULL(result);
  return result;
}


Statement* Parser::ParseStatement(ZoneList<const AstRawString*>* labels,
                                  bool* ok) {
  // Statement ::
  //   EmptyStatement
  //   ...

  if (peek() == Token::SEMICOLON) {
    Next();
    return factory()->NewEmptyStatement(RelocInfo::kNoPosition);
  }
  return ParseSubStatement(labels, ok);
}


Statement* Parser::ParseSubStatement(ZoneList<const AstRawString*>* labels,
                                     bool* ok) {
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
      if (is_strong(language_mode())) {
        ReportMessageAt(scanner()->peek_location(),
                        MessageTemplate::kStrongEmpty);
        *ok = false;
        return NULL;
      }
      Next();
      return factory()->NewEmptyStatement(RelocInfo::kNoPosition);

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
            factory()->NewBlock(labels, 1, false, RelocInfo::kNoPosition);
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

    case Token::FUNCTION: {
      // FunctionDeclaration is only allowed in the context of SourceElements
      // (Ecma 262 5th Edition, clause 14):
      // SourceElement:
      //    Statement
      //    FunctionDeclaration
      // Common language extension is to allow function declaration in place
      // of any statement. This language extension is disabled in strict mode.
      //
      // In Harmony mode, this case also handles the extension:
      // Statement:
      //    GeneratorDeclaration
      if (is_strict(language_mode())) {
        ReportMessageAt(scanner()->peek_location(),
                        MessageTemplate::kStrictFunction);
        *ok = false;
        return NULL;
      }
      return ParseFunctionDeclaration(NULL, ok);
    }

    case Token::DEBUGGER:
      return ParseDebuggerStatement(ok);

    case Token::VAR:
      return ParseVariableStatement(kStatement, NULL, ok);

    case Token::CONST:
      // In ES6 CONST is not allowed as a Statement, only as a
      // LexicalDeclaration, however we continue to allow it in sloppy mode for
      // backwards compatibility.
      if (is_sloppy(language_mode()) && allow_legacy_const()) {
        return ParseVariableStatement(kStatement, NULL, ok);
      }

    // Fall through.
    default:
      return ParseExpressionOrLabelledStatement(labels, ok);
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


VariableProxy* Parser::NewUnresolved(const AstRawString* name,
                                     VariableMode mode) {
  // If we are inside a function, a declaration of a var/const variable is a
  // truly local variable, and the scope of the variable is always the function
  // scope.
  // Let/const variables in harmony mode are always added to the immediately
  // enclosing scope.
  Scope* scope =
      IsLexicalVariableMode(mode) ? scope_ : scope_->DeclarationScope();
  return scope->NewUnresolved(factory(), name, Variable::NORMAL,
                              scanner()->location().beg_pos,
                              scanner()->location().end_pos);
}


Variable* Parser::Declare(Declaration* declaration,
                          DeclarationDescriptor::Kind declaration_kind,
                          bool resolve, bool* ok, Scope* scope) {
  VariableProxy* proxy = declaration->proxy();
  DCHECK(proxy->raw_name() != NULL);
  const AstRawString* name = proxy->raw_name();
  VariableMode mode = declaration->mode();
  bool is_function_declaration = declaration->IsFunctionDeclaration();
  if (scope == nullptr) scope = scope_;
  Scope* declaration_scope =
      IsLexicalVariableMode(mode) ? scope : scope->DeclarationScope();
  Variable* var = NULL;

  // If a suitable scope exists, then we can statically declare this
  // variable and also set its mode. In any case, a Declaration node
  // will be added to the scope so that the declaration can be added
  // to the corresponding activation frame at runtime if necessary.
  // For instance, var declarations inside a sloppy eval scope need
  // to be added to the calling function context. Similarly, strict
  // mode eval scope and lexical eval bindings do not leak variable
  // declarations to the caller's scope so we declare all locals, too.
  if (declaration_scope->is_function_scope() ||
      declaration_scope->is_block_scope() ||
      declaration_scope->is_module_scope() ||
      declaration_scope->is_script_scope() ||
      (declaration_scope->is_eval_scope() &&
       (is_strict(declaration_scope->language_mode()) ||
        IsLexicalVariableMode(mode)))) {
    // Declare the variable in the declaration scope.
    var = declaration_scope->LookupLocal(name);
    if (var == NULL) {
      // Declare the name.
      Variable::Kind kind = Variable::NORMAL;
      if (is_function_declaration) {
        kind = Variable::FUNCTION;
      }
      var = declaration_scope->DeclareLocal(
          name, mode, declaration->initialization(), kind, kNotAssigned);
    } else if ((mode == CONST_LEGACY || var->mode() == CONST_LEGACY) &&
               !declaration_scope->is_script_scope()) {
      // Duplicate legacy const definitions throw at runtime.
      DCHECK(is_sloppy(language_mode()));
      Expression* expression = NewThrowSyntaxError(
          MessageTemplate::kVarRedeclaration, name, declaration->position());
      declaration_scope->SetIllegalRedeclaration(expression);
    } else if ((IsLexicalVariableMode(mode) ||
                IsLexicalVariableMode(var->mode())) &&
               // Lexical bindings may appear for some parameters in sloppy
               // mode even with --harmony-sloppy off.
               (is_strict(language_mode()) || allow_harmony_sloppy())) {
      // Allow duplicate function decls for web compat, see bug 4693.
      if (is_sloppy(language_mode()) && is_function_declaration &&
          var->is_function()) {
        DCHECK(IsLexicalVariableMode(mode) &&
               IsLexicalVariableMode(var->mode()));
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
          ParserTraits::ReportMessage(MessageTemplate::kVarRedeclaration, name);
        } else {
          ParserTraits::ReportMessage(MessageTemplate::kParamDupe);
        }
        *ok = false;
        return nullptr;
      }
    } else if (mode == VAR) {
      var->set_maybe_assigned();
    }
  } else if (declaration_scope->is_eval_scope() &&
             is_sloppy(declaration_scope->language_mode()) &&
             !IsLexicalVariableMode(mode)) {
    // In a var binding in a sloppy direct eval, pollute the enclosing scope
    // with this new binding by doing the following:
    // The proxy is bound to a lookup variable to force a dynamic declaration
    // using the DeclareLookupSlot runtime function.
    Variable::Kind kind = Variable::NORMAL;
    // TODO(sigurds) figure out if kNotAssigned is OK here
    var = new (zone()) Variable(declaration_scope, name, mode, kind,
                                declaration->initialization(), kNotAssigned);
    var->AllocateTo(VariableLocation::LOOKUP, -1);
    var->SetFromEval();
    resolve = true;
  }


  // We add a declaration node for every declaration. The compiler
  // will only generate code if necessary. In particular, declarations
  // for inner local variables that do not represent functions won't
  // result in any generated code.
  //
  // Note that we always add an unresolved proxy even if it's not
  // used, simply because we don't know in this method (w/o extra
  // parameters) if the proxy is needed or not. The proxy will be
  // bound during variable resolution time unless it was pre-bound
  // below.
  //
  // WARNING: This will lead to multiple declaration nodes for the
  // same variable if it is declared several times. This is not a
  // semantic issue as long as we keep the source order, but it may be
  // a performance issue since it may lead to repeated
  // RuntimeHidden_DeclareLookupSlot calls.
  declaration_scope->AddDeclaration(declaration);

  if (mode == CONST_LEGACY && declaration_scope->is_script_scope()) {
    // For global const variables we bind the proxy to a variable.
    DCHECK(resolve);  // should be set by all callers
    Variable::Kind kind = Variable::NORMAL;
    var = new (zone()) Variable(declaration_scope, name, mode, kind,
                                kNeedsInitialization, kNotAssigned);
  }

  // If requested and we have a local variable, bind the proxy to the variable
  // at parse-time. This is used for functions (and consts) declared inside
  // statements: the corresponding function (or const) variable must be in the
  // function scope and not a statement-local scope, e.g. as provided with a
  // 'with' statement:
  //
  //   with (obj) {
  //     function f() {}
  //   }
  //
  // which is translated into:
  //
  //   with (obj) {
  //     // in this case this is not: 'var f; f = function () {};'
  //     var f = function () {};
  //   }
  //
  // Note that if 'f' is accessed from inside the 'with' statement, it
  // will be allocated in the context (because we must be able to look
  // it up dynamically) but it will also be accessed statically, i.e.,
  // with a context slot index and a context chain length for this
  // initialization code. Thus, inside the 'with' statement, we need
  // both access to the static and the dynamic context chain; the
  // runtime needs to provide both.
  if (resolve && var != NULL) {
    proxy->BindTo(var);
  }
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
  // TODO(adamk): Should this be ClosureScope()?
  scope_->DeclarationScope()->ForceEagerCompilation();

  // TODO(1240846): It's weird that native function declarations are
  // introduced dynamically when we meet their declarations, whereas
  // other functions are set up when entering the surrounding scope.
  VariableProxy* proxy = NewUnresolved(name, VAR);
  Declaration* declaration =
      factory()->NewVariableDeclaration(proxy, VAR, scope_, pos);
  Declare(declaration, DeclarationDescriptor::NORMAL, true, CHECK_OK);
  NativeFunctionLiteral* lit = factory()->NewNativeFunctionLiteral(
      name, extension_, RelocInfo::kNoPosition);
  return factory()->NewExpressionStatement(
      factory()->NewAssignment(Token::INIT, proxy, lit, RelocInfo::kNoPosition),
      pos);
}


Statement* Parser::ParseFunctionDeclaration(
    ZoneList<const AstRawString*>* names, bool* ok) {
  Expect(Token::FUNCTION, CHECK_OK);
  int pos = position();
  bool is_generator = Check(Token::MUL);
  return ParseFunctionDeclaration(pos, is_generator, names, ok);
}


Statement* Parser::ParseFunctionDeclaration(
    int pos, bool is_generator, ZoneList<const AstRawString*>* names,
    bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  // GeneratorDeclaration ::
  //   'function' '*' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  //
  // 'function' and '*' (if present) have been consumed by the caller.
  bool is_strict_reserved = false;
  const AstRawString* name = ParseIdentifierOrStrictReservedWord(
      &is_strict_reserved, CHECK_OK);

  FuncNameInferrer::State fni_state(fni_);
  if (fni_ != NULL) fni_->PushEnclosingName(name);
  FunctionLiteral* fun = ParseFunctionLiteral(
      name, scanner()->location(),
      is_strict_reserved ? kFunctionNameIsStrictReserved
                         : kFunctionNameValidityUnknown,
      is_generator ? FunctionKind::kGeneratorFunction
                   : FunctionKind::kNormalFunction,
      pos, FunctionLiteral::kDeclaration, language_mode(), CHECK_OK);

  // Even if we're not at the top-level of the global or a function
  // scope, we treat it as such and introduce the function with its
  // initial value upon entering the corresponding scope.
  // In ES6, a function behaves as a lexical binding, except in
  // a script scope, or the initial scope of eval or another function.
  VariableMode mode =
      is_strong(language_mode())
          ? CONST
          : (is_strict(language_mode()) || allow_harmony_sloppy_function()) &&
                    !scope_->is_declaration_scope()
                ? LET
                : VAR;
  VariableProxy* proxy = NewUnresolved(name, mode);
  Declaration* declaration =
      factory()->NewFunctionDeclaration(proxy, mode, fun, scope_, pos);
  Declare(declaration, DeclarationDescriptor::NORMAL, true, CHECK_OK);
  if (names) names->Add(name, zone());
  EmptyStatement* empty = factory()->NewEmptyStatement(RelocInfo::kNoPosition);
  if (is_sloppy(language_mode()) && allow_harmony_sloppy_function() &&
      !scope_->is_declaration_scope()) {
    SloppyBlockFunctionStatement* delegate =
        factory()->NewSloppyBlockFunctionStatement(empty, scope_);
    scope_->DeclarationScope()->sloppy_block_function_map()->Declare(name,
                                                                     delegate);
    return delegate;
  }
  return empty;
}


Statement* Parser::ParseClassDeclaration(ZoneList<const AstRawString*>* names,
                                         bool* ok) {
  // ClassDeclaration ::
  //   'class' Identifier ('extends' LeftHandExpression)? '{' ClassBody '}'
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

  if (!allow_harmony_sloppy() && is_sloppy(language_mode())) {
    ReportMessage(MessageTemplate::kSloppyLexical);
    *ok = false;
    return NULL;
  }

  int pos = position();
  bool is_strict_reserved = false;
  const AstRawString* name =
      ParseIdentifierOrStrictReservedWord(&is_strict_reserved, CHECK_OK);
  ClassLiteral* value = ParseClassLiteral(name, scanner()->location(),
                                          is_strict_reserved, pos, CHECK_OK);

  VariableMode mode = is_strong(language_mode()) ? CONST : LET;
  VariableProxy* proxy = NewUnresolved(name, mode);
  Declaration* declaration =
      factory()->NewVariableDeclaration(proxy, mode, scope_, pos);
  Declare(declaration, DeclarationDescriptor::NORMAL, true, CHECK_OK);
  proxy->var()->set_initializer_position(position());
  Assignment* assignment =
      factory()->NewAssignment(Token::INIT, proxy, value, pos);
  Statement* assignment_statement =
      factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
  if (names) names->Add(name, zone());
  return assignment_statement;
}


Block* Parser::ParseBlock(ZoneList<const AstRawString*>* labels,
                          bool finalize_block_scope, bool* ok) {
  // The harmony mode uses block elements instead of statements.
  //
  // Block ::
  //   '{' StatementList '}'

  // Construct block expecting 16 statements.
  Block* body =
      factory()->NewBlock(labels, 16, false, RelocInfo::kNoPosition);
  Scope* block_scope = NewScope(scope_, BLOCK_SCOPE);

  // Parse the statements and collect escaping labels.
  Expect(Token::LBRACE, CHECK_OK);
  block_scope->set_start_position(scanner()->location().beg_pos);
  { BlockState block_state(&scope_, block_scope);
    Target target(&this->target_stack_, body);

    while (peek() != Token::RBRACE) {
      Statement* stat = ParseStatementListItem(CHECK_OK);
      if (stat && !stat->IsEmpty()) {
        body->statements()->Add(stat, zone());
      }
    }
  }
  Expect(Token::RBRACE, CHECK_OK);
  block_scope->set_end_position(scanner()->location().end_pos);
  if (finalize_block_scope) {
    block_scope = block_scope->FinalizeBlockScope();
  }
  body->set_scope(block_scope);
  return body;
}


Block* Parser::ParseBlock(ZoneList<const AstRawString*>* labels, bool* ok) {
  return ParseBlock(labels, true, ok);
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

  // The scope of a var/const declared variable anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). Thus we can
  // transform a source-level var/const declaration into a (Function)
  // Scope declaration, and rewrite the source-level initialization into an
  // assignment statement. We use a block to collect multiple assignments.
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
    if (is_strong(language_mode())) {
      Scanner::Location location = scanner()->peek_location();
      ReportMessageAt(location, MessageTemplate::kStrongVar);
      *ok = false;
      return nullptr;
    }
    Consume(Token::VAR);
  } else if (peek() == Token::CONST && allow_const()) {
    Consume(Token::CONST);
    if (is_sloppy(language_mode()) && allow_legacy_const()) {
      parsing_result->descriptor.mode = CONST_LEGACY;
      ++use_counts_[v8::Isolate::kLegacyConst];
    } else {
      DCHECK(is_strict(language_mode()) || allow_harmony_sloppy());
      DCHECK(var_context != kStatement);
      parsing_result->descriptor.mode = CONST;
    }
  } else if (peek() == Token::LET && allow_let()) {
    Consume(Token::LET);
    DCHECK(var_context != kStatement);
    parsing_result->descriptor.mode = LET;
  } else {
    UNREACHABLE();  // by current callers
  }

  parsing_result->descriptor.scope = scope_;
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
      Token::Value next = peek();
      pattern = ParsePrimaryExpression(&pattern_classifier, CHECK_OK);
      ValidateBindingPattern(&pattern_classifier, CHECK_OK);
      if (IsLexicalVariableMode(parsing_result->descriptor.mode)) {
        ValidateLetPattern(&pattern_classifier, CHECK_OK);
      }
      if (!allow_harmony_destructuring_bind() && !pattern->IsVariableProxy()) {
        ReportUnexpectedToken(next);
        *ok = false;
        return nullptr;
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
    int initializer_position = RelocInfo::kNoPosition;
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

      if (allow_harmony_function_name()) {
        ParserTraits::SetFunctionNameFromIdentifierRef(value, pattern);
      }

      // End position of the initializer is after the assignment expression.
      initializer_position = scanner()->location().end_pos;
    } else {
      // Initializers may be either required or implied unless this is a
      // for-in/of iteration variable.
      if (var_context != kForStatement || !PeekInOrOf()) {
        // ES6 'const' and binding patterns require initializers.
        if (parsing_result->descriptor.mode == CONST ||
            !pattern->IsVariableProxy()) {
          ParserTraits::ReportMessageAt(
              Scanner::Location(decl_pos, scanner()->location().end_pos),
              MessageTemplate::kDeclarationMissingInitializer,
              !pattern->IsVariableProxy() ? "destructuring" : "const");
          *ok = false;
          return nullptr;
        }

        // 'let x' and (legacy) 'const x' initialize 'x' to undefined.
        if (parsing_result->descriptor.mode == LET ||
            parsing_result->descriptor.mode == CONST_LEGACY) {
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


Statement* Parser::ParseExpressionOrLabelledStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
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

    case Token::THIS:
      if (!FLAG_strong_this) break;
      // Fall through.
    case Token::SUPER:
      if (is_strong(language_mode()) &&
          IsClassConstructor(function_state_->kind())) {
        bool is_this = peek() == Token::THIS;
        Expression* expr;
        ExpressionClassifier classifier(this);
        if (is_this) {
          expr = ParseStrongInitializationExpression(&classifier, CHECK_OK);
        } else {
          expr = ParseStrongSuperCallExpression(&classifier, CHECK_OK);
        }
        RewriteNonPattern(&classifier, CHECK_OK);
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
              return nullptr;
            }
        }
        return factory()->NewExpressionStatement(expr, pos);
      }
      break;

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
      ParserTraits::ReportMessage(MessageTemplate::kLabelRedeclaration, label);
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
    scope_->RemoveUnresolved(var);
    Expect(Token::COLON, CHECK_OK);
    return ParseStatement(labels, ok);
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
  // Detect attempts at 'let' declarations in sloppy mode.
  if (!allow_harmony_sloppy_let() && peek() == Token::IDENTIFIER &&
      expr->AsVariableProxy() != NULL &&
      expr->AsVariableProxy()->raw_name() ==
          ast_value_factory()->let_string()) {
    ReportMessage(MessageTemplate::kSloppyLexical, NULL);
    *ok = false;
    return NULL;
  }
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
  Statement* then_statement = ParseSubStatement(labels, CHECK_OK);
  Statement* else_statement = NULL;
  if (peek() == Token::ELSE) {
    Next();
    else_statement = ParseSubStatement(labels, CHECK_OK);
  } else {
    else_statement = factory()->NewEmptyStatement(RelocInfo::kNoPosition);
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
    ParserTraits::ReportMessage(message, label);
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
    ParserTraits::ReportMessage(message, label);
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
  function_state_->set_return_location(loc);

  Token::Value tok = peek();
  Statement* result;
  Expression* return_value;
  if (scanner()->HasAnyLineTerminatorBeforeNext() ||
      tok == Token::SEMICOLON ||
      tok == Token::RBRACE ||
      tok == Token::EOS) {
    if (IsSubclassConstructor(function_state_->kind())) {
      return_value = ThisExpression(scope_, factory(), loc.beg_pos);
    } else {
      return_value = GetLiteralUndefined(position());
    }
  } else {
    if (is_strong(language_mode()) &&
        IsClassConstructor(function_state_->kind())) {
      int pos = peek_position();
      ReportMessageAt(Scanner::Location(pos, pos + 1),
                      MessageTemplate::kStrongConstructorReturnValue);
      *ok = false;
      return NULL;
    }

    int pos = peek_position();
    return_value = ParseExpression(true, CHECK_OK);

    if (IsSubclassConstructor(function_state_->kind())) {
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
      Variable* temp = scope_->NewTemporary(
          ast_value_factory()->empty_string());
      Assignment* assign = factory()->NewAssignment(
          Token::ASSIGN, factory()->NewVariableProxy(temp), return_value, pos);

      // %_IsJSReceiver(temp)
      ZoneList<Expression*>* is_spec_object_args =
          new (zone()) ZoneList<Expression*>(1, zone());
      is_spec_object_args->Add(factory()->NewVariableProxy(temp), zone());
      Expression* is_spec_object_call = factory()->NewCallRuntime(
          Runtime::kInlineIsJSReceiver, is_spec_object_args, pos);

      // %_IsJSReceiver(temp) ? temp : throw_expression
      Expression* is_object_conditional = factory()->NewConditional(
          is_spec_object_call, factory()->NewVariableProxy(temp),
          factory()->NewSmiLiteral(1, pos), pos);

      // temp === undefined
      Expression* is_undefined = factory()->NewCompareOperation(
          Token::EQ_STRICT, assign,
          factory()->NewUndefinedLiteral(RelocInfo::kNoPosition), pos);

      // is_undefined ? this : is_object_conditional
      return_value = factory()->NewConditional(
          is_undefined, ThisExpression(scope_, factory(), pos),
          is_object_conditional, pos);
    }

    // ES6 14.6.1 Static Semantics: IsInTailPosition
    if (FLAG_harmony_tailcalls && !is_sloppy(language_mode())) {
      function_state_->AddExpressionInTailPosition(return_value);
    }
  }
  ExpectSemicolon(CHECK_OK);

  if (is_generator()) {
    Expression* generator = factory()->NewVariableProxy(
        function_state_->generator_object_variable());
    Expression* yield = factory()->NewYield(
        generator, return_value, Yield::kFinal, loc.beg_pos);
    result = factory()->NewExpressionStatement(yield, loc.beg_pos);
  } else {
    result = factory()->NewReturnStatement(return_value, loc.beg_pos);
  }

  Scope* decl_scope = scope_->DeclarationScope();
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

  scope_->DeclarationScope()->RecordWithStatement();
  Scope* with_scope = NewScope(scope_, WITH_SCOPE);
  Block* body;
  { BlockState block_state(&scope_, with_scope);
    with_scope->set_start_position(scanner()->peek_location().beg_pos);

    // The body of the with statement must be enclosed in an additional
    // lexical scope in case the body is a FunctionDeclaration.
    body = factory()->NewBlock(labels, 1, false, RelocInfo::kNoPosition);
    Scope* block_scope = NewScope(scope_, BLOCK_SCOPE);
    block_scope->set_start_position(scanner()->location().beg_pos);
    {
      BlockState block_state(&scope_, block_scope);
      Target target(&this->target_stack_, body);
      Statement* stmt = ParseSubStatement(labels, CHECK_OK);
      body->statements()->Add(stmt, zone());
      block_scope->set_end_position(scanner()->location().end_pos);
      block_scope = block_scope->FinalizeBlockScope();
      body->set_scope(block_scope);
    }

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
  if (is_strong(language_mode()) && stat != NULL && !stat->IsJump() &&
      peek() != Token::RBRACE) {
    ReportMessageAt(scanner()->location(),
                    MessageTemplate::kStrongSwitchFallthrough);
    *ok = false;
    return NULL;
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

  Block* switch_block =
      factory()->NewBlock(NULL, 2, false, RelocInfo::kNoPosition);
  int switch_pos = peek_position();

  Expect(Token::SWITCH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* tag = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  Variable* tag_variable =
      scope_->NewTemporary(ast_value_factory()->dot_switch_tag_string());
  Assignment* tag_assign = factory()->NewAssignment(
      Token::ASSIGN, factory()->NewVariableProxy(tag_variable), tag,
      tag->position());
  Statement* tag_statement =
      factory()->NewExpressionStatement(tag_assign, RelocInfo::kNoPosition);
  switch_block->statements()->Add(tag_statement, zone());

  // make statement: undefined;
  // This is needed so the tag isn't returned as the value, in case the switch
  // statements don't have a value.
  switch_block->statements()->Add(
      factory()->NewExpressionStatement(
          factory()->NewUndefinedLiteral(RelocInfo::kNoPosition),
          RelocInfo::kNoPosition),
      zone());

  Block* cases_block =
      factory()->NewBlock(NULL, 1, false, RelocInfo::kNoPosition);
  Scope* cases_scope = NewScope(scope_, BLOCK_SCOPE);
  cases_scope->SetNonlinear();

  SwitchStatement* switch_statement =
      factory()->NewSwitchStatement(labels, switch_pos);

  cases_scope->set_start_position(scanner()->location().beg_pos);
  {
    BlockState cases_block_state(&scope_, cases_scope);
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
  }
  Expect(Token::RBRACE, CHECK_OK);

  cases_scope->set_end_position(scanner()->location().end_pos);
  cases_scope = cases_scope->FinalizeBlockScope();
  cases_block->set_scope(cases_scope);

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

class Parser::DontCollectExpressionsInTailPositionScope {
 public:
  DontCollectExpressionsInTailPositionScope(
      Parser::FunctionState* function_state)
      : function_state_(function_state),
        old_value_(function_state->collect_expressions_in_tail_position()) {
    function_state->set_collect_expressions_in_tail_position(false);
  }
  ~DontCollectExpressionsInTailPositionScope() {
    function_state_->set_collect_expressions_in_tail_position(old_value_);
  }

 private:
  Parser::FunctionState* function_state_;
  bool old_value_;
};

// Collects all return expressions at tail call position in this scope
// to a separate list.
class Parser::CollectExpressionsInTailPositionToListScope {
 public:
  CollectExpressionsInTailPositionToListScope(
      Parser::FunctionState* function_state, List<Expression*>* list)
      : function_state_(function_state), list_(list) {
    function_state->expressions_in_tail_position().Swap(list_);
  }
  ~CollectExpressionsInTailPositionToListScope() {
    function_state_->expressions_in_tail_position().Swap(list_);
  }

 private:
  Parser::FunctionState* function_state_;
  List<Expression*>* list_;
};

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
    DontCollectExpressionsInTailPositionScope no_tail_calls(function_state_);
    try_block = ParseBlock(NULL, CHECK_OK);
  }

  Token::Value tok = peek();
  if (tok != Token::CATCH && tok != Token::FINALLY) {
    ReportMessage(MessageTemplate::kNoCatchOrFinally);
    *ok = false;
    return NULL;
  }

  Scope* catch_scope = NULL;
  Variable* catch_variable = NULL;
  Block* catch_block = NULL;
  List<Expression*> expressions_in_tail_position_in_catch_block;
  if (tok == Token::CATCH) {
    Consume(Token::CATCH);

    Expect(Token::LPAREN, CHECK_OK);
    catch_scope = NewScope(scope_, CATCH_SCOPE);
    catch_scope->set_start_position(scanner()->location().beg_pos);

    ExpressionClassifier pattern_classifier(this);
    Expression* pattern = ParsePrimaryExpression(&pattern_classifier, CHECK_OK);
    ValidateBindingPattern(&pattern_classifier, CHECK_OK);

    const AstRawString* name = ast_value_factory()->dot_catch_string();
    bool is_simple = pattern->IsVariableProxy();
    if (is_simple) {
      auto proxy = pattern->AsVariableProxy();
      scope_->RemoveUnresolved(proxy);
      name = proxy->raw_name();
    }

    catch_variable = catch_scope->DeclareLocal(name, VAR, kCreatedInitialized,
                                               Variable::NORMAL);

    Expect(Token::RPAREN, CHECK_OK);

    {
      CollectExpressionsInTailPositionToListScope
          collect_expressions_in_tail_position_scope(
              function_state_, &expressions_in_tail_position_in_catch_block);
      BlockState block_state(&scope_, catch_scope);

      // TODO(adamk): Make a version of ParseBlock that takes a scope and
      // a block.
      catch_block =
          factory()->NewBlock(nullptr, 16, false, RelocInfo::kNoPosition);
      Scope* block_scope = NewScope(scope_, BLOCK_SCOPE);

      block_scope->set_start_position(scanner()->location().beg_pos);
      {
        BlockState block_state(&scope_, block_scope);
        Target target(&this->target_stack_, catch_block);

        if (!is_simple) {
          DeclarationDescriptor descriptor;
          descriptor.declaration_kind = DeclarationDescriptor::NORMAL;
          descriptor.parser = this;
          descriptor.scope = scope_;
          descriptor.hoist_scope = nullptr;
          descriptor.mode = LET;
          descriptor.declaration_pos = pattern->position();
          descriptor.initialization_pos = pattern->position();

          DeclarationParsingResult::Declaration decl(
              pattern, pattern->position(),
              factory()->NewVariableProxy(catch_variable));

          PatternRewriter::DeclareAndInitializeVariables(
              catch_block, &descriptor, &decl, nullptr, CHECK_OK);
        }

        Expect(Token::LBRACE, CHECK_OK);
        while (peek() != Token::RBRACE) {
          Statement* stat = ParseStatementListItem(CHECK_OK);
          if (stat && !stat->IsEmpty()) {
            catch_block->statements()->Add(stat, zone());
          }
        }
        Consume(Token::RBRACE);
      }
      block_scope->set_end_position(scanner()->location().end_pos);
      block_scope = block_scope->FinalizeBlockScope();
      catch_block->set_scope(block_scope);
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
    TryCatchStatement* statement =
        factory()->NewTryCatchStatement(try_block, catch_scope, catch_variable,
                                        catch_block, RelocInfo::kNoPosition);
    try_block = factory()->NewBlock(NULL, 1, false, RelocInfo::kNoPosition);
    try_block->statements()->Add(statement, zone());
    catch_block = NULL;  // Clear to indicate it's been handled.
  }

  TryStatement* result = NULL;
  if (catch_block != NULL) {
    // For a try-catch construct append return expressions from the catch block
    // to the list of return expressions.
    function_state_->expressions_in_tail_position().AddAll(
        expressions_in_tail_position_in_catch_block);

    DCHECK(finally_block == NULL);
    DCHECK(catch_scope != NULL && catch_variable != NULL);
    result = factory()->NewTryCatchStatement(try_block, catch_scope,
                                             catch_variable, catch_block, pos);
  } else {
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
  Statement* body = ParseSubStatement(NULL, CHECK_OK);
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
  Statement* body = ParseSubStatement(NULL, CHECK_OK);

  if (loop != NULL) loop->Initialize(cond, body);
  return loop;
}


// !%_IsJSReceiver(result = iterator.next()) &&
//     %ThrowIteratorResultNotAnObject(result)
Expression* Parser::BuildIteratorNextResult(Expression* iterator,
                                            Variable* result, int pos) {
  Expression* next_literal = factory()->NewStringLiteral(
      ast_value_factory()->next_string(), RelocInfo::kNoPosition);
  Expression* next_property =
      factory()->NewProperty(iterator, next_literal, RelocInfo::kNoPosition);
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


void Parser::InitializeForEachStatement(ForEachStatement* stmt,
                                        Expression* each, Expression* subject,
                                        Statement* body,
                                        bool is_destructuring) {
  DCHECK(!is_destructuring || allow_harmony_destructuring_assignment());
  ForOfStatement* for_of = stmt->AsForOfStatement();

  if (for_of != NULL) {
    Variable* iterator = scope_->NewTemporary(
        ast_value_factory()->dot_iterator_string());
    Variable* result = scope_->NewTemporary(
        ast_value_factory()->dot_result_string());

    Expression* assign_iterator;
    Expression* next_result;
    Expression* result_done;
    Expression* assign_each;

    // iterator = subject[Symbol.iterator]()
    // Hackily disambiguate o from o.next and o [Symbol.iterator]().
    // TODO(verwaest): Come up with a better solution.
    assign_iterator = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(iterator),
        GetIterator(subject, factory(), subject->position() - 2),
        subject->position());

    // !%_IsJSReceiver(result = iterator.next()) &&
    //     %ThrowIteratorResultNotAnObject(result)
    {
      // result = iterator.next()
      Expression* iterator_proxy = factory()->NewVariableProxy(iterator);
      // Hackily disambiguate o from o.next and o [Symbol.iterator]().
      // TODO(verwaest): Come up with a better solution.
      next_result = BuildIteratorNextResult(iterator_proxy, result,
                                            subject->position() - 1);
    }

    // result.done
    {
      Expression* done_literal = factory()->NewStringLiteral(
          ast_value_factory()->done_string(), RelocInfo::kNoPosition);
      Expression* result_proxy = factory()->NewVariableProxy(result);
      result_done = factory()->NewProperty(
          result_proxy, done_literal, RelocInfo::kNoPosition);
    }

    // each = result.value
    {
      Expression* value_literal = factory()->NewStringLiteral(
          ast_value_factory()->value_string(), RelocInfo::kNoPosition);
      Expression* result_proxy = factory()->NewVariableProxy(result);
      Expression* result_value = factory()->NewProperty(
          result_proxy, value_literal, RelocInfo::kNoPosition);
      assign_each = factory()->NewAssignment(Token::ASSIGN, each, result_value,
                                             RelocInfo::kNoPosition);
      if (is_destructuring) {
        assign_each = PatternRewriter::RewriteDestructuringAssignment(
            this, assign_each->AsAssignment(), scope_);
      }
    }

    for_of->Initialize(each, subject, body,
                       iterator,
                       assign_iterator,
                       next_result,
                       result_done,
                       assign_each);
  } else {
    if (is_destructuring) {
      Variable* temp =
          scope_->NewTemporary(ast_value_factory()->empty_string());
      VariableProxy* temp_proxy = factory()->NewVariableProxy(temp);
      Expression* assign_each = PatternRewriter::RewriteDestructuringAssignment(
          this, factory()->NewAssignment(Token::ASSIGN, each, temp_proxy,
                                         RelocInfo::kNoPosition),
          scope_);
      auto block =
          factory()->NewBlock(nullptr, 2, false, RelocInfo::kNoPosition);
      block->statements()->Add(factory()->NewExpressionStatement(
                                   assign_each, RelocInfo::kNoPosition),
                               zone());
      block->statements()->Add(body, zone());
      body = block;
      each = factory()->NewVariableProxy(temp);
    }
    stmt->Initialize(each, subject, body);
  }
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

  Block* outer_block = factory()->NewBlock(NULL, names->length() + 4, false,
                                           RelocInfo::kNoPosition);

  // Add statement: let/const x = i.
  outer_block->statements()->Add(init, zone());

  const AstRawString* temp_name = ast_value_factory()->dot_for_string();

  // For each lexical variable x:
  //   make statement: temp_x = x.
  for (int i = 0; i < names->length(); i++) {
    VariableProxy* proxy = NewUnresolved(names->at(i), LET);
    Variable* temp = scope_->NewTemporary(temp_name);
    VariableProxy* temp_proxy = factory()->NewVariableProxy(temp);
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, temp_proxy, proxy, RelocInfo::kNoPosition);
    Statement* assignment_statement = factory()->NewExpressionStatement(
        assignment, RelocInfo::kNoPosition);
    outer_block->statements()->Add(assignment_statement, zone());
    temps.Add(temp, zone());
  }

  Variable* first = NULL;
  // Make statement: first = 1.
  if (next) {
    first = scope_->NewTemporary(temp_name);
    VariableProxy* first_proxy = factory()->NewVariableProxy(first);
    Expression* const1 = factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, first_proxy, const1, RelocInfo::kNoPosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
    outer_block->statements()->Add(assignment_statement, zone());
  }

  // make statement: undefined;
  outer_block->statements()->Add(
      factory()->NewExpressionStatement(
          factory()->NewUndefinedLiteral(RelocInfo::kNoPosition),
          RelocInfo::kNoPosition),
      zone());

  // Make statement: outer: for (;;)
  // Note that we don't actually create the label, or set this loop up as an
  // explicit break target, instead handing it directly to those nodes that
  // need to know about it. This should be safe because we don't run any code
  // in this function that looks up break targets.
  ForStatement* outer_loop =
      factory()->NewForStatement(NULL, RelocInfo::kNoPosition);
  outer_block->statements()->Add(outer_loop, zone());
  outer_block->set_scope(scope_);

  Block* inner_block =
      factory()->NewBlock(NULL, 3, false, RelocInfo::kNoPosition);
  {
    BlockState block_state(&scope_, inner_scope);

    Block* ignore_completion_block = factory()->NewBlock(
        NULL, names->length() + 3, true, RelocInfo::kNoPosition);
    ZoneList<Variable*> inner_vars(names->length(), zone());
    // For each let variable x:
    //    make statement: let/const x = temp_x.
    for (int i = 0; i < names->length(); i++) {
      VariableProxy* proxy = NewUnresolved(names->at(i), mode);
      Declaration* declaration = factory()->NewVariableDeclaration(
          proxy, mode, scope_, RelocInfo::kNoPosition);
      Declare(declaration, DeclarationDescriptor::NORMAL, true, CHECK_OK);
      inner_vars.Add(declaration->proxy()->var(), zone());
      VariableProxy* temp_proxy = factory()->NewVariableProxy(temps.at(i));
      Assignment* assignment = factory()->NewAssignment(
          Token::INIT, proxy, temp_proxy, RelocInfo::kNoPosition);
      Statement* assignment_statement =
          factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
      DCHECK(init->position() != RelocInfo::kNoPosition);
      proxy->var()->set_initializer_position(init->position());
      ignore_completion_block->statements()->Add(assignment_statement, zone());
    }

    // Make statement: if (first == 1) { first = 0; } else { next; }
    if (next) {
      DCHECK(first);
      Expression* compare = NULL;
      // Make compare expression: first == 1.
      {
        Expression* const1 =
            factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
        VariableProxy* first_proxy = factory()->NewVariableProxy(first);
        compare = factory()->NewCompareOperation(Token::EQ, first_proxy, const1,
                                                 RelocInfo::kNoPosition);
      }
      Statement* clear_first = NULL;
      // Make statement: first = 0.
      {
        VariableProxy* first_proxy = factory()->NewVariableProxy(first);
        Expression* const0 =
            factory()->NewSmiLiteral(0, RelocInfo::kNoPosition);
        Assignment* assignment = factory()->NewAssignment(
            Token::ASSIGN, first_proxy, const0, RelocInfo::kNoPosition);
        clear_first = factory()->NewExpressionStatement(assignment,
                                                        RelocInfo::kNoPosition);
      }
      Statement* clear_first_or_next = factory()->NewIfStatement(
          compare, clear_first, next, RelocInfo::kNoPosition);
      ignore_completion_block->statements()->Add(clear_first_or_next, zone());
    }

    Variable* flag = scope_->NewTemporary(temp_name);
    // Make statement: flag = 1.
    {
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      Expression* const1 = factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
      Assignment* assignment = factory()->NewAssignment(
          Token::ASSIGN, flag_proxy, const1, RelocInfo::kNoPosition);
      Statement* assignment_statement =
          factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
      ignore_completion_block->statements()->Add(assignment_statement, zone());
    }

    // Make statement: if (!cond) break.
    if (cond) {
      Statement* stop =
          factory()->NewBreakStatement(outer_loop, RelocInfo::kNoPosition);
      Statement* noop = factory()->NewEmptyStatement(RelocInfo::kNoPosition);
      ignore_completion_block->statements()->Add(
          factory()->NewIfStatement(cond, noop, stop, cond->position()),
          zone());
    }

    inner_block->statements()->Add(ignore_completion_block, zone());
    // Make cond expression for main loop: flag == 1.
    Expression* flag_cond = NULL;
    {
      Expression* const1 = factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      flag_cond = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                                 RelocInfo::kNoPosition);
    }

    // Create chain of expressions "flag = 0, temp_x = x, ..."
    Statement* compound_next_statement = NULL;
    {
      Expression* compound_next = NULL;
      // Make expression: flag = 0.
      {
        VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
        Expression* const0 =
            factory()->NewSmiLiteral(0, RelocInfo::kNoPosition);
        compound_next = factory()->NewAssignment(
            Token::ASSIGN, flag_proxy, const0, RelocInfo::kNoPosition);
      }

      // Make the comma-separated list of temp_x = x assignments.
      int inner_var_proxy_pos = scanner()->location().beg_pos;
      for (int i = 0; i < names->length(); i++) {
        VariableProxy* temp_proxy = factory()->NewVariableProxy(temps.at(i));
        VariableProxy* proxy =
            factory()->NewVariableProxy(inner_vars.at(i), inner_var_proxy_pos);
        Assignment* assignment = factory()->NewAssignment(
            Token::ASSIGN, temp_proxy, proxy, RelocInfo::kNoPosition);
        compound_next = factory()->NewBinaryOperation(
            Token::COMMA, compound_next, assignment, RelocInfo::kNoPosition);
      }

      compound_next_statement = factory()->NewExpressionStatement(
          compound_next, RelocInfo::kNoPosition);
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
        Expression* const1 =
            factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
        VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
        compare = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                                 RelocInfo::kNoPosition);
      }
      Statement* stop =
          factory()->NewBreakStatement(outer_loop, RelocInfo::kNoPosition);
      Statement* empty = factory()->NewEmptyStatement(RelocInfo::kNoPosition);
      Statement* if_flag_break = factory()->NewIfStatement(
          compare, stop, empty, RelocInfo::kNoPosition);
      Block* ignore_completion_block =
          factory()->NewBlock(NULL, 1, true, RelocInfo::kNoPosition);
      ignore_completion_block->statements()->Add(if_flag_break, zone());
      inner_block->statements()->Add(ignore_completion_block, zone());
    }

    inner_scope->set_end_position(scanner()->location().end_pos);
    inner_block->set_scope(inner_scope);
  }

  outer_loop->Initialize(NULL, NULL, NULL, inner_block);
  return outer_block;
}


Statement* Parser::ParseForStatement(ZoneList<const AstRawString*>* labels,
                                     bool* ok) {
  int stmt_pos = peek_position();
  Statement* init = NULL;
  ZoneList<const AstRawString*> lexical_bindings(1, zone());

  // Create an in-between scope for let-bound iteration variables.
  Scope* for_scope = NewScope(scope_, BLOCK_SCOPE);

  BlockState block_state(&scope_, for_scope);
  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  for_scope->set_start_position(scanner()->location().beg_pos);
  bool is_let_identifier_expression = false;
  DeclarationParsingResult parsing_result;
  if (peek() != Token::SEMICOLON) {
    if (peek() == Token::VAR || (peek() == Token::CONST && allow_const()) ||
        (peek() == Token::LET && IsNextLetKeyword())) {
      ParseVariableDeclarations(kForStatement, &parsing_result, nullptr,
                                CHECK_OK);

      ForEachStatement::VisitMode mode;
      int each_beg_pos = scanner()->location().beg_pos;
      int each_end_pos = scanner()->location().end_pos;

      if (CheckInOrOf(&mode, ok)) {
        if (!*ok) return nullptr;
        if (parsing_result.declarations.length() != 1) {
          ParserTraits::ReportMessageAt(
              parsing_result.bindings_loc,
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
             !decl.pattern->IsVariableProxy())) {
          ParserTraits::ReportMessageAt(
              parsing_result.first_initializer_loc,
              MessageTemplate::kForInOfLoopInitializer,
              ForEachStatement::VisitModeString(mode));
          *ok = false;
          return nullptr;
        }

        Block* init_block = nullptr;

        // special case for legacy for (var/const x =.... in)
        if (!IsLexicalVariableMode(parsing_result.descriptor.mode) &&
            decl.pattern->IsVariableProxy() && decl.initializer != nullptr) {
          ++use_counts_[v8::Isolate::kForInInitializer];
          const AstRawString* name =
              decl.pattern->AsVariableProxy()->raw_name();
          VariableProxy* single_var = scope_->NewUnresolved(
              factory(), name, Variable::NORMAL, each_beg_pos, each_end_pos);
          init_block = factory()->NewBlock(
              nullptr, 2, true, parsing_result.descriptor.declaration_pos);
          init_block->statements()->Add(
              factory()->NewExpressionStatement(
                  factory()->NewAssignment(Token::ASSIGN, single_var,
                                           decl.initializer,
                                           RelocInfo::kNoPosition),
                  RelocInfo::kNoPosition),
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

        Variable* temp =
            scope_->NewTemporary(ast_value_factory()->dot_for_string());
        ForEachStatement* loop =
            factory()->NewForEachStatement(mode, labels, stmt_pos);
        Target target(&this->target_stack_, loop);

        Expression* enumerable;
        if (mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          enumerable = ParseAssignmentExpression(true, &classifier, CHECK_OK);
          RewriteNonPattern(&classifier, CHECK_OK);
        } else {
          enumerable = ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);

        Scope* body_scope = NewScope(scope_, BLOCK_SCOPE);
        body_scope->set_start_position(scanner()->location().beg_pos);

        Block* body_block =
            factory()->NewBlock(NULL, 3, false, RelocInfo::kNoPosition);

        {
          BlockState block_state(&scope_, body_scope);

          Statement* body = ParseSubStatement(NULL, CHECK_OK);

          auto each_initialization_block =
              factory()->NewBlock(nullptr, 1, true, RelocInfo::kNoPosition);
          {
            auto descriptor = parsing_result.descriptor;
            descriptor.declaration_pos = RelocInfo::kNoPosition;
            descriptor.initialization_pos = RelocInfo::kNoPosition;
            decl.initializer = factory()->NewVariableProxy(temp);

            PatternRewriter::DeclareAndInitializeVariables(
                each_initialization_block, &descriptor, &decl,
                IsLexicalVariableMode(descriptor.mode) ? &lexical_bindings
                                                       : nullptr,
                CHECK_OK);
          }

          body_block->statements()->Add(each_initialization_block, zone());
          body_block->statements()->Add(body, zone());
          VariableProxy* temp_proxy =
              factory()->NewVariableProxy(temp, each_beg_pos, each_end_pos);
          InitializeForEachStatement(loop, temp_proxy, enumerable, body_block,
                                     false);
        }
        body_scope->set_end_position(scanner()->location().end_pos);
        body_scope = body_scope->FinalizeBlockScope();
        body_block->set_scope(body_scope);

        // Create a TDZ for any lexically-bound names.
        if (IsLexicalVariableMode(parsing_result.descriptor.mode)) {
          DCHECK_NULL(init_block);

          init_block =
              factory()->NewBlock(nullptr, 1, false, RelocInfo::kNoPosition);

          for (int i = 0; i < lexical_bindings.length(); ++i) {
            // TODO(adamk): This needs to be some sort of special
            // INTERNAL variable that's invisible to the debugger
            // but visible to everything else.
            VariableProxy* tdz_proxy =
                NewUnresolved(lexical_bindings[i], LET);
            Declaration* tdz_decl = factory()->NewVariableDeclaration(
                tdz_proxy, LET, scope_, RelocInfo::kNoPosition);
            Variable* tdz_var = Declare(
                tdz_decl, DeclarationDescriptor::NORMAL, true, CHECK_OK);
            tdz_var->set_initializer_position(position());
          }
        }

        Statement* final_loop = loop->IsForOfStatement()
            ? FinalizeForOfStatement(
                loop->AsForOfStatement(), RelocInfo::kNoPosition)
            : loop;

        for_scope->set_end_position(scanner()->location().end_pos);
        for_scope = for_scope->FinalizeBlockScope();
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
        init = parsing_result.BuildInitializationBlock(
            IsLexicalVariableMode(parsing_result.descriptor.mode)
                ? &lexical_bindings
                : nullptr,
            CHECK_OK);
      }
    } else {
      int lhs_beg_pos = peek_position();
      ExpressionClassifier classifier(this);
      Expression* expression = ParseExpression(false, &classifier, CHECK_OK);
      int lhs_end_pos = scanner()->location().end_pos;
      ForEachStatement::VisitMode mode;
      is_let_identifier_expression =
          expression->IsVariableProxy() &&
          expression->AsVariableProxy()->raw_name() ==
              ast_value_factory()->let_string();

      bool is_for_each = CheckInOrOf(&mode, ok);
      if (!*ok) return nullptr;
      bool is_destructuring =
          is_for_each && allow_harmony_destructuring_assignment() &&
          (expression->IsArrayLiteral() || expression->IsObjectLiteral());

      if (is_destructuring) {
        ValidateAssignmentPattern(&classifier, CHECK_OK);
      } else {
        RewriteNonPattern(&classifier, CHECK_OK);
      }

      if (is_for_each) {
        if (!is_destructuring) {
          expression = this->CheckAndRewriteReferenceExpression(
              expression, lhs_beg_pos, lhs_end_pos,
              MessageTemplate::kInvalidLhsInFor, kSyntaxError, CHECK_OK);
        }

        ForEachStatement* loop =
            factory()->NewForEachStatement(mode, labels, stmt_pos);
        Target target(&this->target_stack_, loop);

        Expression* enumerable;
        if (mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          enumerable = ParseAssignmentExpression(true, &classifier, CHECK_OK);
          RewriteNonPattern(&classifier, CHECK_OK);
        } else {
          enumerable = ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);

        // Make a block around the statement in case a lexical binding
        // is introduced, e.g. by a FunctionDeclaration.
        // This block must not use for_scope as its scope because if a
        // lexical binding is introduced which overlaps with the for-in/of,
        // expressions in head of the loop should actually have variables
        // resolved in the outer scope.
        Scope* body_scope = NewScope(for_scope, BLOCK_SCOPE);
        {
          BlockState block_state(&scope_, body_scope);
          Block* block =
              factory()->NewBlock(NULL, 1, false, RelocInfo::kNoPosition);
          Statement* body = ParseSubStatement(NULL, CHECK_OK);
          block->statements()->Add(body, zone());
          InitializeForEachStatement(loop, expression, enumerable, block,
                                     is_destructuring);
          body_scope->set_end_position(scanner()->location().end_pos);
          body_scope = body_scope->FinalizeBlockScope();
          block->set_scope(body_scope);
        }

        Statement* final_loop = loop->IsForOfStatement()
            ? FinalizeForOfStatement(
                loop->AsForOfStatement(), RelocInfo::kNoPosition)
            : loop;

        for_scope->set_end_position(scanner()->location().end_pos);
        for_scope = for_scope->FinalizeBlockScope();
        DCHECK(for_scope == nullptr);
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
  // Detect attempts at 'let' declarations in sloppy mode.
  if (!allow_harmony_sloppy_let() && peek() == Token::IDENTIFIER &&
      is_sloppy(language_mode()) && is_let_identifier_expression) {
    ReportMessage(MessageTemplate::kSloppyLexical, NULL);
    *ok = false;
    return NULL;
  }
  Expect(Token::SEMICOLON, CHECK_OK);

  Expression* cond = NULL;
  Statement* next = NULL;
  Statement* body = NULL;

  // If there are let bindings, then condition and the next statement of the
  // for loop must be parsed in a new scope.
  Scope* inner_scope = scope_;
  if (lexical_bindings.length() > 0) {
    inner_scope = NewScope(for_scope, BLOCK_SCOPE);
    inner_scope->set_start_position(scanner()->location().beg_pos);
  }
  {
    BlockState block_state(&scope_, inner_scope);

    if (peek() != Token::SEMICOLON) {
      cond = ParseExpression(true, CHECK_OK);
    }
    Expect(Token::SEMICOLON, CHECK_OK);

    if (peek() != Token::RPAREN) {
      Expression* exp = ParseExpression(true, CHECK_OK);
      next = factory()->NewExpressionStatement(exp, exp->position());
    }
    Expect(Token::RPAREN, CHECK_OK);

    body = ParseSubStatement(NULL, CHECK_OK);
  }

  Statement* result = NULL;
  if (lexical_bindings.length() > 0) {
    BlockState block_state(&scope_, for_scope);
    result = DesugarLexicalBindingsInForStatement(
        inner_scope, parsing_result.descriptor.mode, &lexical_bindings, loop,
        init, cond, next, body, CHECK_OK);
    for_scope->set_end_position(scanner()->location().end_pos);
  } else {
    for_scope->set_end_position(scanner()->location().end_pos);
    for_scope = for_scope->FinalizeBlockScope();
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
      Block* block =
          factory()->NewBlock(NULL, 2, false, RelocInfo::kNoPosition);
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


void ParserTraits::ParseArrowFunctionFormalParameters(
    ParserFormalParameters* parameters, Expression* expr,
    const Scanner::Location& params_loc, bool* ok) {
  if (parameters->Arity() >= Code::kMaxArguments) {
    ReportMessageAt(params_loc, MessageTemplate::kMalformedArrowFunParamList);
    *ok = false;
    return;
  }

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
    ParseArrowFunctionFormalParameters(parameters, left, params_loc, ok);
    if (!*ok) return;
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
  if (expr->IsVariableProxy()) {
    // When the formal parameter was originally seen, it was parsed as a
    // VariableProxy and recorded as unresolved in the scope.  Here we undo that
    // parse-time side-effect for parameters that are single-names (not
    // patterns; for patterns that happens uniformly in
    // PatternRewriter::VisitVariableProxy).
    parser_->scope_->RemoveUnresolved(expr->AsVariableProxy());
  } else if (expr->IsAssignment()) {
    Assignment* assignment = expr->AsAssignment();
    DCHECK(parser_->allow_harmony_default_parameters());
    DCHECK(!assignment->is_compound());
    initializer = assignment->value();
    expr = assignment->target();

    // TODO(adamk): Only call this if necessary.
    RewriteParameterInitializerScope(parser_->stack_limit(), initializer,
                                     parser_->scope_, parameters->scope);
  }

  // TODO(adamk): params_loc.end_pos is not the correct initializer position,
  // but it should be conservative enough to trigger hole checks for variables
  // referenced in the initializer (if any).
  AddFormalParameter(parameters, expr, initializer, params_loc.end_pos,
                     is_rest);
}


DoExpression* Parser::ParseDoExpression(bool* ok) {
  // AssignmentExpression ::
  //     do '{' StatementList '}'
  int pos = peek_position();

  Expect(Token::DO, CHECK_OK);
  Variable* result =
      scope_->NewTemporary(ast_value_factory()->dot_result_string());
  Block* block = ParseBlock(nullptr, false, CHECK_OK);
  DoExpression* expr = factory()->NewDoExpression(block, result, pos);
  if (!Rewriter::Rewrite(this, expr, ast_value_factory())) {
    *ok = false;
    return nullptr;
  }
  block->set_scope(block->scope()->FinalizeBlockScope());
  return expr;
}


void ParserTraits::ParseArrowFunctionFormalParameterList(
    ParserFormalParameters* parameters, Expression* expr,
    const Scanner::Location& params_loc,
    Scanner::Location* duplicate_loc, bool* ok) {
  if (expr->IsEmptyParentheses()) return;

  ParseArrowFunctionFormalParameters(parameters, expr, params_loc, ok);
  if (!*ok) return;

  Type::ExpressionClassifier classifier(parser_);
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


void ParserTraits::ReindexLiterals(const ParserFormalParameters& parameters) {
  if (parser_->function_state_->materialized_literal_count() > 0) {
    AstLiteralReindexer reindexer;

    for (const auto p : parameters.params) {
      if (p.pattern != nullptr) reindexer.Reindex(p.pattern);
      if (p.initializer != nullptr) reindexer.Reindex(p.initializer);
    }

    DCHECK(reindexer.count() <=
           parser_->function_state_->materialized_literal_count());
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

  int pos = function_token_pos == RelocInfo::kNoPosition
      ? peek_position() : function_token_pos;

  bool is_generator = IsGeneratorFunction(kind);

  // Anonymous functions were passed either the empty symbol or a null
  // handle as the function name.  Remember if we were passed a non-empty
  // handle to decide whether to invoke function name inference.
  bool should_infer_name = function_name == NULL;

  // We want a non-null handle as the function name.
  if (should_infer_name) {
    function_name = ast_value_factory()->empty_string();
  }

  // Function declarations are function scoped in normal mode, so they are
  // hoisted. In harmony block scoping mode they are block scoped, so they
  // are not hoisted.
  //
  // One tricky case are function declarations in a local sloppy-mode eval:
  // their declaration is hoisted, but they still see the local scope. E.g.,
  //
  // function() {
  //   var x = 0
  //   try { throw 1 } catch (x) { eval("function g() { return x }") }
  //   return g()
  // }
  //
  // needs to return 1. To distinguish such cases, we need to detect
  // (1) whether a function stems from a sloppy eval, and
  // (2) whether it actually hoists across the eval.
  // Unfortunately, we do not represent sloppy eval scopes, so we do not have
  // either information available directly, especially not when lazily compiling
  // a function like 'g'. We hence rely on the following invariants:
  // - (1) is the case iff the innermost scope of the deserialized scope chain
  //   under which we compile is _not_ a declaration scope. This holds because
  //   in all normal cases, function declarations are fully hoisted to a
  //   declaration scope and compiled relative to that.
  // - (2) is the case iff the current declaration scope is still the original
  //   one relative to the deserialized scope chain. Otherwise we must be
  //   compiling a function in an inner declaration scope in the eval, e.g. a
  //   nested function, and hoisting works normally relative to that.
  Scope* declaration_scope = scope_->DeclarationScope();
  Scope* original_declaration_scope = original_scope_->DeclarationScope();
  Scope* scope = function_type == FunctionLiteral::kDeclaration &&
                         is_sloppy(language_mode) &&
                         !allow_harmony_sloppy_function() &&
                         (original_scope_ == original_declaration_scope ||
                          declaration_scope != original_declaration_scope)
                     ? NewScope(declaration_scope, FUNCTION_SCOPE, kind)
                     : NewScope(scope_, FUNCTION_SCOPE, kind);
  SetLanguageMode(scope, language_mode);
  ZoneList<Statement*>* body = NULL;
  int arity = -1;
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  DuplicateFinder duplicate_finder(scanner()->unicode_cache());
  FunctionLiteral::EagerCompileHint eager_compile_hint =
      parenthesized_function_ ? FunctionLiteral::kShouldEagerCompile
                              : FunctionLiteral::kShouldLazyCompile;
  bool should_be_used_once_hint = false;
  bool has_duplicate_parameters;
  // Parse function.
  {
    AstNodeFactory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, scope, kind,
                                 &function_factory);
    scope_->SetScopeName(function_name);
    ExpressionClassifier formals_classifier(this, &duplicate_finder);

    if (is_generator) {
      // For generators, allocating variables in contexts is currently a win
      // because it minimizes the work needed to suspend and resume an
      // activation.
      scope_->ForceContextAllocation();

      // Calling a generator returns a generator object.  That object is stored
      // in a temporary variable, a definition that is used by "yield"
      // expressions. This also marks the FunctionState as a generator.
      Variable* temp = scope_->NewTemporary(
          ast_value_factory()->dot_generator_object_string());
      function_state.set_generator_object_variable(temp);
    }

    Expect(Token::LPAREN, CHECK_OK);
    int start_position = scanner()->location().beg_pos;
    scope_->set_start_position(start_position);
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

    // Determine if the function can be parsed lazily. Lazy parsing is different
    // from lazy compilation; we need to parse more eagerly than we compile.

    // We can only parse lazily if we also compile lazily. The heuristics for
    // lazy compilation are:
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
                            scope_->AllowsLazyParsing() &&
                            !parenthesized_function_;
    parenthesized_function_ = false;  // The bit was set for this function only.

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
      bool use_temp_zone =
          FLAG_lazy && !allow_natives() && extension_ == NULL && allow_lazy() &&
          function_type == FunctionLiteral::kDeclaration &&
          eager_compile_hint != FunctionLiteral::kShouldEagerCompile;
      // Open a new BodyScope, which sets our AstNodeFactory to allocate in the
      // new temporary zone if the preconditions are satisfied, and ensures that
      // the previous zone is always restored after parsing the body.
      // For the purpose of scope analysis, some ZoneObjects allocated by the
      // factory must persist after the function body is thrown away and
      // temp_zone is deallocated. These objects are instead allocated in a
      // parser-persistent zone (see parser_zone_ in AstNodeFactory).
      {
        Zone temp_zone;
        AstNodeFactory::BodyScope inner(factory(), &temp_zone, use_temp_zone);

        body = ParseEagerFunctionBody(function_name, pos, formals, kind,
                                      function_type, CHECK_OK);
      }
      materialized_literal_count = function_state.materialized_literal_count();
      expected_property_count = function_state.expected_property_count();
      if (use_temp_zone) {
        // If the preconditions are correct the function body should never be
        // accessed, but do this anyway for better behaviour if they're wrong.
        body = NULL;
      }
    }

    // Parsing the body may change the language mode in our scope.
    language_mode = scope->language_mode();

    if (is_strong(language_mode) && IsSubclassConstructor(kind)) {
      if (!function_state.super_location().IsValid()) {
        ReportMessageAt(function_name_location,
                        MessageTemplate::kStrongSuperCallMissing,
                        kReferenceError);
        *ok = false;
        return nullptr;
      }
    }

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
    }
    if (is_sloppy(language_mode) && allow_harmony_sloppy_function()) {
      InsertSloppyBlockFunctionVarBindings(scope, CHECK_OK);
    }
    if (is_strict(language_mode) || allow_harmony_sloppy() ||
        allow_harmony_destructuring_bind()) {
      CheckConflictingVarDeclarations(scope, CHECK_OK);
    }

    if (body) {
      // If body can be inspected, rewrite queued destructuring assignments
      ParserTraits::RewriteDestructuringAssignments();
    }
    has_duplicate_parameters =
      !formals_classifier.is_valid_formal_parameter_list_without_duplicates();
  }

  FunctionLiteral::ParameterFlag duplicate_parameters =
      has_duplicate_parameters ? FunctionLiteral::kHasDuplicateParameters
                               : FunctionLiteral::kNoDuplicateParameters;

  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      function_name, scope, body, materialized_literal_count,
      expected_property_count, arity, duplicate_parameters, function_type,
      eager_compile_hint, kind, pos);
  function_literal->set_function_token_position(function_token_pos);
  if (should_be_used_once_hint)
    function_literal->set_should_be_used_once_hint();

  if (fni_ != NULL && should_infer_name) fni_->AddFunction(function_literal);
  return function_literal;
}


void Parser::SkipLazyFunctionBody(int* materialized_literal_count,
                                  int* expected_property_count, bool* ok,
                                  Scanner::BookmarkScope* bookmark) {
  DCHECK_IMPLIES(bookmark, bookmark->HasBeenSet());
  if (produce_cached_parse_data()) CHECK(log_);

  int function_block_pos = position();
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

      scope_->set_end_position(entry.end_pos());
      Expect(Token::RBRACE, ok);
      if (!*ok) {
        return;
      }
      total_preparse_skipped_ += scope_->end_position() - function_block_pos;
      *materialized_literal_count = entry.literal_count();
      *expected_property_count = entry.property_count();
      SetLanguageMode(scope_, entry.language_mode());
      if (entry.uses_super_property()) scope_->RecordSuperPropertyUsage();
      if (entry.calls_eval()) scope_->RecordEvalCall();
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
    ParserTraits::ReportMessageAt(
        Scanner::Location(logger.start(), logger.end()), logger.message(),
        logger.argument_opt(), logger.error_type());
    *ok = false;
    return;
  }
  scope_->set_end_position(logger.end());
  Expect(Token::RBRACE, ok);
  if (!*ok) {
    return;
  }
  total_preparse_skipped_ += scope_->end_position() - function_block_pos;
  *materialized_literal_count = logger.literals();
  *expected_property_count = logger.properties();
  SetLanguageMode(scope_, logger.language_mode());
  if (logger.uses_super_property()) {
    scope_->RecordSuperPropertyUsage();
  }
  if (logger.calls_eval()) {
    scope_->RecordEvalCall();
  }
  if (produce_cached_parse_data()) {
    DCHECK(log_);
    // Position right after terminal '}'.
    int body_end = scanner()->location().end_pos;
    log_->LogFunction(function_block_pos, body_end, *materialized_literal_count,
                      *expected_property_count, scope_->language_mode(),
                      scope_->uses_super_property(), scope_->calls_eval());
  }
}


Statement* Parser::BuildAssertIsCoercible(Variable* var) {
  // if (var === null || var === undefined)
  //     throw /* type error kNonCoercible) */;

  Expression* condition = factory()->NewBinaryOperation(
      Token::OR, factory()->NewCompareOperation(
                     Token::EQ_STRICT, factory()->NewVariableProxy(var),
                     factory()->NewUndefinedLiteral(RelocInfo::kNoPosition),
                     RelocInfo::kNoPosition),
      factory()->NewCompareOperation(
          Token::EQ_STRICT, factory()->NewVariableProxy(var),
          factory()->NewNullLiteral(RelocInfo::kNoPosition),
          RelocInfo::kNoPosition),
      RelocInfo::kNoPosition);
  Expression* throw_type_error = this->NewThrowTypeError(
      MessageTemplate::kNonCoercible, ast_value_factory()->empty_string(),
      RelocInfo::kNoPosition);
  IfStatement* if_statement = factory()->NewIfStatement(
      condition, factory()->NewExpressionStatement(throw_type_error,
                                                   RelocInfo::kNoPosition),
      factory()->NewEmptyStatement(RelocInfo::kNoPosition),
      RelocInfo::kNoPosition);
  return if_statement;
}


class InitializerRewriter : public AstExpressionVisitor {
 public:
  InitializerRewriter(uintptr_t stack_limit, Expression* root, Parser* parser,
                      Scope* scope)
      : AstExpressionVisitor(stack_limit, root),
        parser_(parser),
        scope_(scope) {}

 private:
  void VisitExpression(Expression* expr) override {
    RewritableExpression* to_rewrite = expr->AsRewritableExpression();
    if (to_rewrite == nullptr || to_rewrite->is_rewritten()) return;

    Parser::PatternRewriter::RewriteDestructuringAssignment(parser_, to_rewrite,
                                                            scope_);
  }

  // Code in function literals does not need to be eagerly rewritten, it will be
  // rewritten when scheduled.
  void VisitFunctionLiteral(FunctionLiteral* expr) override {}

 private:
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
  DCHECK(scope_->is_function_scope());
  Block* init_block =
      factory()->NewBlock(NULL, 1, true, RelocInfo::kNoPosition);
  for (int i = 0; i < parameters.params.length(); ++i) {
    auto parameter = parameters.params[i];
    if (parameter.is_rest && parameter.pattern->IsVariableProxy()) break;
    DeclarationDescriptor descriptor;
    descriptor.declaration_kind = DeclarationDescriptor::PARAMETER;
    descriptor.parser = this;
    descriptor.scope = scope_;
    descriptor.hoist_scope = nullptr;
    descriptor.mode = LET;
    descriptor.declaration_pos = parameter.pattern->position();
    // The position that will be used by the AssignmentExpression
    // which copies from the temp parameter to the pattern.
    //
    // TODO(adamk): Should this be RelocInfo::kNoPosition, since
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
      RewriteParameterInitializer(parameter.initializer, scope_);

      auto condition = factory()->NewCompareOperation(
          Token::EQ_STRICT,
          factory()->NewVariableProxy(parameters.scope->parameter(i)),
          factory()->NewUndefinedLiteral(RelocInfo::kNoPosition),
          RelocInfo::kNoPosition);
      initial_value = factory()->NewConditional(
          condition, parameter.initializer, initial_value,
          RelocInfo::kNoPosition);
      descriptor.initialization_pos = parameter.initializer->position();
      initializer_position = parameter.initializer_end_position;
    }

    Scope* param_scope = scope_;
    Block* param_block = init_block;
    if (!parameter.is_simple() && scope_->calls_sloppy_eval()) {
      param_scope = NewScope(scope_, BLOCK_SCOPE);
      param_scope->set_is_declaration_scope();
      param_scope->set_start_position(parameter.pattern->position());
      param_scope->set_end_position(RelocInfo::kNoPosition);
      param_scope->RecordEvalCall();
      param_block = factory()->NewBlock(NULL, 8, true, RelocInfo::kNoPosition);
      param_block->set_scope(param_scope);
      descriptor.hoist_scope = scope_;
    }

    {
      BlockState block_state(&scope_, param_scope);
      DeclarationParsingResult::Declaration decl(
          parameter.pattern, initializer_position, initial_value);
      PatternRewriter::DeclareAndInitializeVariables(param_block, &descriptor,
                                                     &decl, nullptr, CHECK_OK);
    }

    if (!parameter.is_simple() && scope_->calls_sloppy_eval()) {
      param_scope = param_scope->FinalizeBlockScope();
      if (param_scope != nullptr) {
        CheckConflictingVarDeclarations(param_scope, CHECK_OK);
      }
      init_block->statements()->Add(param_block, zone());
    }
  }
  return init_block;
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
  Scope* inner_scope = scope_;
  Block* inner_block = nullptr;
  if (!parameters.is_simple) {
    inner_scope = NewScope(scope_, BLOCK_SCOPE);
    inner_scope->set_is_declaration_scope();
    inner_scope->set_start_position(scanner()->location().beg_pos);
    inner_block = factory()->NewBlock(NULL, 8, true, RelocInfo::kNoPosition);
    inner_block->set_scope(inner_scope);
    body = inner_block->statements();
  }

  {
    BlockState block_state(&scope_, inner_scope);

    if (IsGeneratorFunction(kind)) {
      // We produce:
      //
      // try { InitialYield; ...body...; FinalYield }
      // finally { %GeneratorClose(generator) }
      //
      // - InitialYield yields the actual generator object.
      // - FinalYield yields {value: foo, done: true} where foo is the
      //   completion value of body.  (This is needed here in case the body
      //   falls through without an explicit return.)
      // - Any return statement inside the body will be converted into a similar
      //   FinalYield.
      // - If the generator terminates for whatever reason, we must close it.
      //   Hence the finally clause.

      Block* try_block =
          factory()->NewBlock(nullptr, 3, false, RelocInfo::kNoPosition);

      {
        ZoneList<Expression*>* arguments =
            new (zone()) ZoneList<Expression*>(0, zone());
        CallRuntime* allocation = factory()->NewCallRuntime(
            Runtime::kCreateJSGeneratorObject, arguments, pos);
        VariableProxy* init_proxy = factory()->NewVariableProxy(
            function_state_->generator_object_variable());
        Assignment* assignment = factory()->NewAssignment(
            Token::INIT, init_proxy, allocation, RelocInfo::kNoPosition);
        VariableProxy* get_proxy = factory()->NewVariableProxy(
            function_state_->generator_object_variable());
        Yield* yield = factory()->NewYield(
            get_proxy, assignment, Yield::kInitial, RelocInfo::kNoPosition);
        try_block->statements()->Add(
            factory()->NewExpressionStatement(yield, RelocInfo::kNoPosition),
            zone());
      }

      ParseStatementList(try_block->statements(), Token::RBRACE, CHECK_OK);

      VariableProxy* get_proxy = factory()->NewVariableProxy(
          function_state_->generator_object_variable());
      Expression* undefined =
          factory()->NewUndefinedLiteral(RelocInfo::kNoPosition);
      Yield* yield = factory()->NewYield(get_proxy, undefined, Yield::kFinal,
                                         RelocInfo::kNoPosition);
      try_block->statements()->Add(
          factory()->NewExpressionStatement(yield, RelocInfo::kNoPosition),
          zone());

      Block* finally_block =
          factory()->NewBlock(nullptr, 1, false, RelocInfo::kNoPosition);
      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(1, zone());
      VariableProxy* call_proxy = factory()->NewVariableProxy(
          function_state_->generator_object_variable());
      args->Add(call_proxy, zone());
      Expression* call = factory()->NewCallRuntime(
          Runtime::kGeneratorClose, args, RelocInfo::kNoPosition);
      finally_block->statements()->Add(
          factory()->NewExpressionStatement(call, RelocInfo::kNoPosition),
          zone());

      body->Add(factory()->NewTryFinallyStatement(try_block, finally_block,
                                                  RelocInfo::kNoPosition),
                zone());
    } else {
      ParseStatementList(body, Token::RBRACE, CHECK_OK);
    }

    if (IsSubclassConstructor(kind)) {
      body->Add(
          factory()->NewReturnStatement(
              this->ThisExpression(scope_, factory(), RelocInfo::kNoPosition),
              RelocInfo::kNoPosition),
              zone());
    }
  }

  Expect(Token::RBRACE, CHECK_OK);
  scope_->set_end_position(scanner()->location().end_pos);

  if (!parameters.is_simple) {
    DCHECK_NOT_NULL(inner_scope);
    DCHECK_EQ(body, inner_block->statements());
    SetLanguageMode(scope_, inner_scope->language_mode());
    Block* init_block = BuildParameterInitializationBlock(parameters, CHECK_OK);
    DCHECK_NOT_NULL(init_block);

    inner_scope->set_end_position(scanner()->location().end_pos);
    inner_scope = inner_scope->FinalizeBlockScope();
    if (inner_scope != nullptr) {
      CheckConflictingVarDeclarations(inner_scope, CHECK_OK);
      InsertShadowingVarBindingInitializers(inner_block);
    }

    result->Add(init_block, zone());
    result->Add(inner_block, zone());
  }

  if (function_type == FunctionLiteral::kNamedExpression) {
    // Now that we know the language mode, we can create the const assignment
    // in the previously reserved spot.
    // NOTE: We create a proxy and resolve it here so that in the
    // future we can change the AST to only refer to VariableProxies
    // instead of Variables and Proxies as is the case now.
    VariableMode fvar_mode = is_strict(language_mode()) ? CONST : CONST_LEGACY;
    Variable* fvar = new (zone())
        Variable(scope_, function_name, fvar_mode, Variable::NORMAL,
                 kCreatedInitialized, kNotAssigned);
    VariableProxy* proxy = factory()->NewVariableProxy(fvar);
    VariableDeclaration* fvar_declaration = factory()->NewVariableDeclaration(
        proxy, fvar_mode, scope_, RelocInfo::kNoPosition);
    scope_->DeclareFunctionVar(fvar_declaration);

    VariableProxy* fproxy = factory()->NewVariableProxy(fvar);
    result->Set(kFunctionNameAssignmentIndex,
                factory()->NewExpressionStatement(
                    factory()->NewAssignment(Token::INIT, fproxy,
                                             factory()->NewThisFunction(pos),
                                             RelocInfo::kNoPosition),
                    RelocInfo::kNoPosition));
  }

  // ES6 14.6.1 Static Semantics: IsInTailPosition
  // Mark collected return expressions that are in tail call position.
  const List<Expression*>& expressions_in_tail_position =
      function_state_->expressions_in_tail_position();
  for (int i = 0; i < expressions_in_tail_position.length(); ++i) {
    expressions_in_tail_position[i]->MarkTail();
  }
  return result;
}


PreParser::PreParseResult Parser::ParseLazyFunctionBodyWithPreParser(
    SingletonLogger* logger, Scanner::BookmarkScope* bookmark) {
  // This function may be called on a background thread too; record only the
  // main thread preparse times.
  if (pre_parse_timer_ != NULL) {
    pre_parse_timer_->Start();
  }
  TRACE_EVENT0("v8", "V8.PreParse");

  DCHECK_EQ(Token::LBRACE, scanner()->current_token());

  if (reusable_preparser_ == NULL) {
    reusable_preparser_ = new PreParser(zone(), &scanner_, ast_value_factory(),
                                        NULL, stack_limit_);
    reusable_preparser_->set_allow_lazy(true);
#define SET_ALLOW(name) reusable_preparser_->set_allow_##name(allow_##name());
    SET_ALLOW(natives);
    SET_ALLOW(harmony_sloppy);
    SET_ALLOW(harmony_sloppy_let);
    SET_ALLOW(harmony_default_parameters);
    SET_ALLOW(harmony_destructuring_bind);
    SET_ALLOW(harmony_destructuring_assignment);
    SET_ALLOW(strong_mode);
    SET_ALLOW(harmony_do_expressions);
    SET_ALLOW(harmony_function_name);
    SET_ALLOW(harmony_function_sent);
#undef SET_ALLOW
  }
  PreParser::PreParseResult result = reusable_preparser_->PreParseLazyFunction(
      language_mode(), function_state_->kind(), scope_->has_simple_parameters(),
      logger, bookmark);
  if (pre_parse_timer_ != NULL) {
    pre_parse_timer_->Stop();
  }
  return result;
}


ClassLiteral* Parser::ParseClassLiteral(const AstRawString* name,
                                        Scanner::Location class_name_location,
                                        bool name_is_strict_reserved, int pos,
                                        bool* ok) {
  // All parts of a ClassDeclaration and ClassExpression are strict code.
  if (name_is_strict_reserved) {
    ReportMessageAt(class_name_location,
                    MessageTemplate::kUnexpectedStrictReserved);
    *ok = false;
    return NULL;
  }
  if (IsEvalOrArguments(name)) {
    ReportMessageAt(class_name_location, MessageTemplate::kStrictEvalArguments);
    *ok = false;
    return NULL;
  }
  if (is_strong(language_mode()) && IsUndefined(name)) {
    ReportMessageAt(class_name_location, MessageTemplate::kStrongUndefined);
    *ok = false;
    return NULL;
  }

  Scope* block_scope = NewScope(scope_, BLOCK_SCOPE);
  BlockState block_state(&scope_, block_scope);
  RaiseLanguageMode(STRICT);
  scope_->SetScopeName(name);

  VariableProxy* proxy = NULL;
  if (name != NULL) {
    proxy = NewUnresolved(name, CONST);
    Declaration* declaration =
        factory()->NewVariableDeclaration(proxy, CONST, block_scope, pos);
    Declare(declaration, DeclarationDescriptor::NORMAL, true, CHECK_OK);
  }

  Expression* extends = NULL;
  if (Check(Token::EXTENDS)) {
    block_scope->set_start_position(scanner()->location().end_pos);
    ExpressionClassifier classifier(this);
    extends = ParseLeftHandSideExpression(&classifier, CHECK_OK);
    RewriteNonPattern(&classifier, CHECK_OK);
  } else {
    block_scope->set_start_position(scanner()->location().end_pos);
  }


  ClassLiteralChecker checker(this);
  ZoneList<ObjectLiteral::Property*>* properties = NewPropertyList(4, zone());
  FunctionLiteral* constructor = NULL;
  bool has_seen_constructor = false;

  Expect(Token::LBRACE, CHECK_OK);

  const bool has_extends = extends != nullptr;
  while (peek() != Token::RBRACE) {
    if (Check(Token::SEMICOLON)) continue;
    FuncNameInferrer::State fni_state(fni_);
    const bool in_class = true;
    const bool is_static = false;
    bool is_computed_name = false;  // Classes do not care about computed
                                    // property names here.
    ExpressionClassifier classifier(this);
    const AstRawString* property_name = nullptr;
    ObjectLiteral::Property* property = ParsePropertyDefinition(
        &checker, in_class, has_extends, is_static, &is_computed_name,
        &has_seen_constructor, &classifier, &property_name, CHECK_OK);
    RewriteNonPattern(&classifier, CHECK_OK);

    if (has_seen_constructor && constructor == NULL) {
      constructor = GetPropertyValue(property)->AsFunctionLiteral();
      DCHECK_NOT_NULL(constructor);
      constructor->set_raw_name(
          name != nullptr ? name : ast_value_factory()->empty_string());
    } else {
      properties->Add(property, zone());
    }

    if (fni_ != NULL) fni_->Infer();

    if (allow_harmony_function_name() &&
        property_name != ast_value_factory()->constructor_string()) {
      SetFunctionNameFromPropertyName(property, property_name);
    }
  }

  Expect(Token::RBRACE, CHECK_OK);
  int end_pos = scanner()->location().end_pos;

  if (constructor == NULL) {
    constructor = DefaultConstructor(name, extends != NULL, block_scope, pos,
                                     end_pos, block_scope->language_mode());
  }

  // Note that we do not finalize this block scope because strong
  // mode uses it as a sentinel value indicating an anonymous class.
  block_scope->set_end_position(end_pos);

  if (name != NULL) {
    DCHECK_NOT_NULL(proxy);
    proxy->var()->set_initializer_position(end_pos);
  }

  return factory()->NewClassLiteral(block_scope, proxy, extends, constructor,
                                    properties, pos, end_pos);
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
    scope_->DeclarationScope()->ForceEagerCompilation();
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
      ReportMessage(MessageTemplate::kIllegalAccess);
      *ok = false;
      return NULL;
    }

    return factory()->NewCallRuntime(function, args, pos);
  }

  int context_index = Context::IntrinsicIndexForName(name->string());

  // Check that the function is defined.
  if (context_index == Context::kNotFound) {
    ParserTraits::ReportMessage(MessageTemplate::kNotDefined, name);
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
    Scanner::Location location = position == RelocInfo::kNoPosition
        ? Scanner::Location::invalid()
        : Scanner::Location(position, position + 1);
    ParserTraits::ReportMessageAt(location, MessageTemplate::kVarRedeclaration,
                                  name);
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
  for (int i = 0; i < decls->length(); ++i) {
    Declaration* decl = decls->at(i);
    if (decl->mode() != VAR || !decl->IsVariableDeclaration()) continue;
    const AstRawString* name = decl->proxy()->raw_name();
    Variable* parameter = function_scope->LookupLocal(name);
    if (parameter == nullptr) continue;
    VariableProxy* to = inner_scope->NewUnresolved(factory(), name);
    VariableProxy* from = factory()->NewVariableProxy(parameter);
    Expression* assignment = factory()->NewAssignment(
        Token::ASSIGN, to, from, RelocInfo::kNoPosition);
    Statement* statement = factory()->NewExpressionStatement(
        assignment, RelocInfo::kNoPosition);
    inner_block->statements()->InsertAt(0, statement, zone());
  }
}


void Parser::InsertSloppyBlockFunctionVarBindings(Scope* scope, bool* ok) {
  // For each variable which is used as a function declaration in a sloppy
  // block,
  DCHECK(scope->is_declaration_scope());
  SloppyBlockFunctionMap* map = scope->sloppy_block_function_map();
  for (ZoneHashMap::Entry* p = map->Start(); p != nullptr; p = map->Next(p)) {
    AstRawString* name = static_cast<AstRawString*>(p->key);
    // If the variable wouldn't conflict with a lexical declaration,
    Variable* var = scope->LookupLocal(name);
    if (var == nullptr || !IsLexicalVariableMode(var->mode())) {
      // Declare a var-style binding for the function in the outer scope
      VariableProxy* proxy = scope->NewUnresolved(factory(), name);
      Declaration* declaration = factory()->NewVariableDeclaration(
          proxy, VAR, scope, RelocInfo::kNoPosition);
      Declare(declaration, DeclarationDescriptor::NORMAL, true, ok, scope);
      DCHECK(ok);  // Based on the preceding check, this should not fail
      if (!ok) return;

      // Write in assignments to var for each block-scoped function declaration
      auto delegates = static_cast<SloppyBlockFunctionMap::Vector*>(p->value);
      for (SloppyBlockFunctionStatement* delegate : *delegates) {
        // Read from the local lexical scope and write to the function scope
        VariableProxy* to = scope->NewUnresolved(factory(), name);
        VariableProxy* from = delegate->scope()->NewUnresolved(factory(), name);
        Expression* assignment = factory()->NewAssignment(
            Token::ASSIGN, to, from, RelocInfo::kNoPosition);
        Statement* statement = factory()->NewExpressionStatement(
            assignment, RelocInfo::kNoPosition);
        delegate->set_statement(statement);
      }
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
  if (scanner_.source_url()->length() > 0) {
    Handle<String> source_url = scanner_.source_url()->Internalize(isolate);
    script->set_source_url(*source_url);
  }
  if (scanner_.source_mapping_url()->length() > 0) {
    Handle<String> source_mapping_url =
        scanner_.source_mapping_url()->Internalize(isolate);
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
    for (int i = 0; i < use_counts_[feature]; ++i) {
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
  fni_ = new (zone()) FuncNameInferrer(ast_value_factory(), zone());

  CompleteParserRecorder recorder;
  if (produce_cached_parse_data()) log_ = &recorder;

  DCHECK(info->source_stream() != NULL);
  ExternalStreamingStream stream(info->source_stream(),
                                 info->source_stream_encoding());
  scanner_.Initialize(&stream);
  DCHECK(info->context().is_null() || info->context()->IsNativeContext());

  // When streaming, we don't know the length of the source until we have parsed
  // it. The raw data can be UTF-8, so we wouldn't know the source length until
  // we have decoded it anyway even if we knew the raw data length (which we
  // don't). We work around this by storing all the scopes which need their end
  // position set at the end of the script (the top scope and possible eval
  // scopes) and set their end position after we know the script length.
  result = DoParseProgram(info);

  info->set_literal(result);

  // We cannot internalize on a background thread; a foreground task will take
  // care of calling Parser::Internalize just before compilation.

  if (produce_cached_parse_data()) {
    if (result != NULL) *info->cached_data() = recorder.GetScriptData();
    log_ = NULL;
  }
}


ParserTraits::TemplateLiteralState Parser::OpenTemplateLiteral(int pos) {
  return new (zone()) ParserTraits::TemplateLiteral(zone(), pos);
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
                  cooked_idx, is_strong(language_mode()), pos),
              zone());
    args->Add(
        factory()->NewArrayLiteral(
            const_cast<ZoneList<Expression*>*>(raw_strings), raw_idx,
            is_strong(language_mode()), pos),
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
                                        spread_list, RelocInfo::kNoPosition),
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
                                             is_strong(language_mode()),
                                             RelocInfo::kNoPosition),
                  zone());

        if (i == n) break;
      }

      // Push eagerly spread argument
      ZoneList<v8::internal::Expression*>* spread_list =
          new (zone()) ZoneList<v8::internal::Expression*>(1, zone());
      spread_list->Add(list->at(i++)->AsSpread()->expression(), zone());
      args->Add(factory()->NewCallRuntime(Context::SPREAD_ITERABLE_INDEX,
                                          spread_list, RelocInfo::kNoPosition),
                zone());
    }

    list = new (zone()) ZoneList<v8::internal::Expression*>(1, zone());
    list->Add(factory()->NewCallRuntime(Context::SPREAD_ARGUMENTS_INDEX, args,
                                        RelocInfo::kNoPosition),
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
        Expression* home =
            ThisExpression(scope_, factory(), RelocInfo::kNoPosition);
        args->InsertAt(0, function, zone());
        args->InsertAt(1, home, zone());
      } else {
        Variable* temp =
            scope_->NewTemporary(ast_value_factory()->empty_string());
        VariableProxy* obj = factory()->NewVariableProxy(temp);
        Assignment* assign_obj = factory()->NewAssignment(
            Token::ASSIGN, obj, function->AsProperty()->obj(),
            RelocInfo::kNoPosition);
        function = factory()->NewProperty(
            assign_obj, function->AsProperty()->key(), RelocInfo::kNoPosition);
        args->InsertAt(0, function, zone());
        obj = factory()->NewVariableProxy(temp);
        args->InsertAt(1, obj, zone());
      }
    } else {
      // Non-method calls
      args->InsertAt(0, function, zone());
      args->InsertAt(1, factory()->NewUndefinedLiteral(RelocInfo::kNoPosition),
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
  else if (is_strong(mode))
    feature = v8::Isolate::kStrongMode;
  else if (is_strict(mode))
    feature = v8::Isolate::kStrictMode;
  else
    UNREACHABLE();
  ++use_counts_[feature];
  scope->SetLanguageMode(mode);
}


void Parser::RaiseLanguageMode(LanguageMode mode) {
  SetLanguageMode(scope_,
                  static_cast<LanguageMode>(scope_->language_mode() | mode));
}


void ParserTraits::RewriteDestructuringAssignments() {
  parser_->RewriteDestructuringAssignments();
}


void ParserTraits::RewriteNonPattern(Type::ExpressionClassifier* classifier,
                                     bool* ok) {
  parser_->RewriteNonPattern(classifier, ok);
}


Zone* ParserTraits::zone() const {
  return parser_->function_state_->scope()->zone();
}


ZoneList<Expression*>* ParserTraits::GetNonPatternList() const {
  return parser_->function_state_->non_patterns_to_rewrite();
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
  ValidateExpression(classifier, ok);
  if (!*ok) return;
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
  if (!allow_harmony_destructuring_assignment()) return;
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
  Variable* result =
      scope_->NewTemporary(ast_value_factory()->dot_result_string());
  // NOTE: The value assigned to R is the whole original array literal,
  // spreads included. This will be fixed before the rewritten AST is returned.
  // $R = lit
  Expression* init_result =
      factory()->NewAssignment(Token::INIT, factory()->NewVariableProxy(result),
                               lit, RelocInfo::kNoPosition);
  Block* do_block =
      factory()->NewBlock(nullptr, 16, false, RelocInfo::kNoPosition);
  do_block->statements()->Add(
      factory()->NewExpressionStatement(init_result, RelocInfo::kNoPosition),
      zone());
  // Traverse the array literal starting from the first spread.
  while (s != lit->EndValue()) {
    Expression* value = *s++;
    Spread* spread = value->AsSpread();
    if (spread == nullptr) {
      // If the element is not a spread, we're adding a single:
      // %AppendElement($R, value)
      ZoneList<Expression*>* append_element_args = NewExpressionList(2, zone());
      append_element_args->Add(factory()->NewVariableProxy(result), zone());
      append_element_args->Add(value, zone());
      do_block->statements()->Add(
          factory()->NewExpressionStatement(
              factory()->NewCallRuntime(Runtime::kAppendElement,
                                        append_element_args,
                                        RelocInfo::kNoPosition),
              RelocInfo::kNoPosition),
          zone());
    } else {
      // If it's a spread, we're adding a for/of loop iterating through it.
      Variable* each =
          scope_->NewTemporary(ast_value_factory()->dot_for_string());
      Expression* subject = spread->expression();
      Variable* iterator =
          scope_->NewTemporary(ast_value_factory()->dot_iterator_string());
      Variable* element =
          scope_->NewTemporary(ast_value_factory()->dot_result_string());
      // iterator = subject[Symbol.iterator]()
      Expression* assign_iterator = factory()->NewAssignment(
          Token::ASSIGN, factory()->NewVariableProxy(iterator),
          GetIterator(subject, factory(), spread->expression_position()),
          subject->position());
      // !%_IsJSReceiver(element = iterator.next()) &&
      //     %ThrowIteratorResultNotAnObject(element)
      Expression* next_element;
      {
        // element = iterator.next()
        Expression* iterator_proxy = factory()->NewVariableProxy(iterator);
        next_element = BuildIteratorNextResult(iterator_proxy, element,
                                               spread->expression_position());
      }
      // element.done
      Expression* element_done;
      {
        Expression* done_literal = factory()->NewStringLiteral(
            ast_value_factory()->done_string(), RelocInfo::kNoPosition);
        Expression* element_proxy = factory()->NewVariableProxy(element);
        element_done = factory()->NewProperty(element_proxy, done_literal,
                                              RelocInfo::kNoPosition);
      }
      // each = element.value
      Expression* assign_each;
      {
        Expression* value_literal = factory()->NewStringLiteral(
            ast_value_factory()->value_string(), RelocInfo::kNoPosition);
        Expression* element_proxy = factory()->NewVariableProxy(element);
        Expression* element_value = factory()->NewProperty(
            element_proxy, value_literal, RelocInfo::kNoPosition);
        assign_each = factory()->NewAssignment(
            Token::ASSIGN, factory()->NewVariableProxy(each), element_value,
            RelocInfo::kNoPosition);
      }
      // %AppendElement($R, each)
      Statement* append_body;
      {
        ZoneList<Expression*>* append_element_args =
            NewExpressionList(2, zone());
        append_element_args->Add(factory()->NewVariableProxy(result), zone());
        append_element_args->Add(factory()->NewVariableProxy(each), zone());
        append_body = factory()->NewExpressionStatement(
            factory()->NewCallRuntime(Runtime::kAppendElement,
                                      append_element_args,
                                      RelocInfo::kNoPosition),
            RelocInfo::kNoPosition);
      }
      // for (each of spread) %AppendElement($R, each)
      ForEachStatement* loop = factory()->NewForEachStatement(
          ForEachStatement::ITERATE, nullptr, RelocInfo::kNoPosition);
      ForOfStatement* for_of = loop->AsForOfStatement();
      for_of->Initialize(factory()->NewVariableProxy(each), subject,
                         append_body, iterator, assign_iterator, next_element,
                         element_done, assign_each);
      do_block->statements()->Add(for_of, zone());
    }
  }
  // Now, rewind the original array literal to truncate everything from the
  // first spread (included) until the end. This fixes $R's initialization.
  lit->RewindSpreads();
  return factory()->NewDoExpression(do_block, result, lit->position());
}


void ParserTraits::QueueDestructuringAssignmentForRewriting(Expression* expr) {
  DCHECK(expr->IsRewritableExpression());
  parser_->function_state_->AddDestructuringAssignment(
      Parser::DestructuringAssignment(expr, parser_->scope_));
}


void ParserTraits::QueueNonPatternForRewriting(Expression* expr) {
  DCHECK(expr->IsRewritableExpression());
  parser_->function_state_->AddNonPatternForRewriting(expr);
}


void ParserTraits::SetFunctionNameFromPropertyName(
    ObjectLiteralProperty* property, const AstRawString* name) {
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
          is_getter ? parser_->ast_value_factory()->get_space_string()
                    : parser_->ast_value_factory()->set_space_string();
      function->set_raw_name(
          parser_->ast_value_factory()->NewConsString(prefix, name));
      return;
    }
  }

  if (!value->IsAnonymousFunctionDefinition()) return;
  DCHECK_NOT_NULL(name);

  // Ignore "__proto__" as a name when it's being used to set the [[Prototype]]
  // of an object literal.
  if (property->kind() == ObjectLiteralProperty::PROTOTYPE) return;

  if (function != nullptr) {
    function->set_raw_name(name);
    DCHECK_EQ(ObjectLiteralProperty::COMPUTED, property->kind());
  } else {
    DCHECK(value->IsClassLiteral());
    DCHECK_EQ(ObjectLiteralProperty::COMPUTED, property->kind());
    value->AsClassLiteral()->constructor()->set_raw_name(name);
  }
}


void ParserTraits::SetFunctionNameFromIdentifierRef(Expression* value,
                                                    Expression* identifier) {
  if (!value->IsAnonymousFunctionDefinition()) return;
  if (!identifier->IsVariableProxy()) return;

  auto name = identifier->AsVariableProxy()->raw_name();
  DCHECK_NOT_NULL(name);

  auto function = value->AsFunctionLiteral();
  if (function != nullptr) {
    function->set_raw_name(name);
  } else {
    DCHECK(value->IsClassLiteral());
    value->AsClassLiteral()->constructor()->set_raw_name(name);
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
//     output.value;
//   }
//
// IteratorClose(iterator) expands to the following:
//
//   let iteratorReturn = iterator.return;
//   if (IS_NULL_OR_UNDEFINED(iteratorReturn)) return;
//   let output = %_Call(iteratorReturn, iterator);
//   if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
//
// IteratorClose(iterator, input, output) expands to the following:
//
//   let iteratorReturn = iterator.return;
//   if (IS_NULL_OR_UNDEFINED(iteratorReturn)) return input;
//   output = %_Call(iteratorReturn, iterator, input);
//   if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);


Expression* ParserTraits::RewriteYieldStar(
    Expression* generator, Expression* iterable, int pos) {

  const int nopos = RelocInfo::kNoPosition;

  auto factory = parser_->factory();
  auto avfactory = parser_->ast_value_factory();
  auto scope = parser_->scope_;
  auto zone = parser_->zone();


  // Forward definition for break/continue statements.
  WhileStatement* loop = factory->NewWhileStatement(nullptr, nopos);


  // let input = undefined;
  Variable* var_input = scope->NewTemporary(avfactory->empty_string());
  Statement* initialize_input;
  {
    Expression* input_proxy = factory->NewVariableProxy(var_input);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, input_proxy, factory->NewUndefinedLiteral(nopos), nopos);
    initialize_input = factory->NewExpressionStatement(assignment, nopos);
  }


  // let mode = kNext;
  Variable* var_mode = scope->NewTemporary(avfactory->empty_string());
  Statement* initialize_mode;
  {
    Expression* mode_proxy = factory->NewVariableProxy(var_mode);
    Expression* knext = factory->NewSmiLiteral(JSGeneratorObject::NEXT, nopos);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, mode_proxy, knext, nopos);
    initialize_mode = factory->NewExpressionStatement(assignment, nopos);
  }


  // let output = undefined;
  Variable* var_output = scope->NewTemporary(avfactory->empty_string());
  Statement* initialize_output;
  {
    Expression* output_proxy = factory->NewVariableProxy(var_output);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, output_proxy, factory->NewUndefinedLiteral(nopos),
        nopos);
    initialize_output = factory->NewExpressionStatement(assignment, nopos);
  }


  // let iterator = iterable[Symbol.iterator];
  Variable* var_iterator = scope->NewTemporary(avfactory->empty_string());
  Statement* get_iterator;
  {
    Expression* iterator = GetIterator(iterable, factory, nopos);
    Expression* iterator_proxy = factory->NewVariableProxy(var_iterator);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, iterator_proxy, iterator, nopos);
    get_iterator = factory->NewExpressionStatement(assignment, nopos);
  }


  // if (!IS_RECEIVER(iterator)) throw MakeTypeError(kSymbolIteratorInvalid);
  Statement* validate_iterator;
  {
    Expression* is_receiver_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_iterator), zone);
      is_receiver_call =
          factory->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      Expression* call = NewThrowTypeError(
          MessageTemplate::kSymbolIteratorInvalid, avfactory->empty_string(),
          nopos);
      throw_call = factory->NewExpressionStatement(call, nopos);
    }

    validate_iterator = factory->NewIfStatement(
        is_receiver_call, factory->NewEmptyStatement(nopos), throw_call, nopos);
  }


  // output = iterator.next(input);
  Statement* call_next;
  {
    Expression* iterator_proxy = factory->NewVariableProxy(var_iterator);
    Expression* literal =
        factory->NewStringLiteral(avfactory->next_string(), nopos);
    Expression* next_property =
        factory->NewProperty(iterator_proxy, literal, nopos);
    Expression* input_proxy = factory->NewVariableProxy(var_input);
    auto args = new (zone) ZoneList<Expression*>(1, zone);
    args->Add(input_proxy, zone);
    Expression* call = factory->NewCall(next_property, args, nopos);
    Expression* output_proxy = factory->NewVariableProxy(var_output);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, output_proxy, call, nopos);
    call_next = factory->NewExpressionStatement(assignment, nopos);
  }


  // if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
  Statement* validate_next_output;
  {
    Expression* is_receiver_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      is_receiver_call =
          factory->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      Expression* call = factory->NewCallRuntime(
          Runtime::kThrowIteratorResultNotAnObject, args, nopos);
      throw_call = factory->NewExpressionStatement(call, nopos);
    }

    validate_next_output = factory->NewIfStatement(
        is_receiver_call, factory->NewEmptyStatement(nopos), throw_call, nopos);
  }


  // let iteratorThrow = iterator.throw;
  Variable* var_throw = scope->NewTemporary(avfactory->empty_string());
  Statement* get_throw;
  {
    Expression* iterator_proxy = factory->NewVariableProxy(var_iterator);
    Expression* literal =
        factory->NewStringLiteral(avfactory->throw_string(), nopos);
    Expression* property =
        factory->NewProperty(iterator_proxy, literal, nopos);
    Expression* throw_proxy = factory->NewVariableProxy(var_throw);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, throw_proxy, property, nopos);
    get_throw = factory->NewExpressionStatement(assignment, nopos);
  }


  // if (IS_NULL_OR_UNDEFINED(iteratorThrow) {
  //   IteratorClose(iterator);
  //   throw MakeTypeError(kThrowMethodMissing);
  // }
  Statement* check_throw;
  {
    Expression* condition = factory->NewCompareOperation(
        Token::EQ, factory->NewVariableProxy(var_throw),
        factory->NewNullLiteral(nopos), nopos);

    Expression* call = NewThrowTypeError(
        MessageTemplate::kThrowMethodMissing,
        avfactory->empty_string(), nopos);
    Statement* throw_call = factory->NewExpressionStatement(call, nopos);

    Block* then = factory->NewBlock(nullptr, 4+1, false, nopos);
    Variable* var_tmp = scope->NewTemporary(avfactory->empty_string());
    BuildIteratorClose(
        then->statements(), var_iterator, factory->NewUndefinedLiteral(nopos),
        var_tmp);
    then->statements()->Add(throw_call, zone);
    check_throw = factory->NewIfStatement(
        condition, then, factory->NewEmptyStatement(nopos), nopos);
  }


  // output = %_Call(iteratorThrow, iterator, input);
  Statement* call_throw;
  {
    auto args = new (zone) ZoneList<Expression*>(3, zone);
    args->Add(factory->NewVariableProxy(var_throw), zone);
    args->Add(factory->NewVariableProxy(var_iterator), zone);
    args->Add(factory->NewVariableProxy(var_input), zone);
    Expression* call =
        factory->NewCallRuntime(Runtime::kInlineCall, args, nopos);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, factory->NewVariableProxy(var_output), call, nopos);
    call_throw = factory->NewExpressionStatement(assignment, nopos);
  }


  // if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
  Statement* validate_throw_output;
  {
    Expression* is_receiver_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      is_receiver_call =
          factory->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      Expression* call = factory->NewCallRuntime(
          Runtime::kThrowIteratorResultNotAnObject, args, nopos);
      throw_call = factory->NewExpressionStatement(call, nopos);
    }

    validate_throw_output = factory->NewIfStatement(
        is_receiver_call, factory->NewEmptyStatement(nopos), throw_call, nopos);
  }


  // if (output.done) break;
  Statement* if_done;
  {
    Expression* output_proxy = factory->NewVariableProxy(var_output);
    Expression* literal =
        factory->NewStringLiteral(avfactory->done_string(), nopos);
    Expression* property = factory->NewProperty(output_proxy, literal, nopos);
    BreakStatement* break_loop = factory->NewBreakStatement(loop, nopos);
    if_done = factory->NewIfStatement(
        property, break_loop, factory->NewEmptyStatement(nopos), nopos);
  }


  // mode = kReturn;
  Statement* set_mode_return;
  {
    Expression* mode_proxy = factory->NewVariableProxy(var_mode);
    Expression* kreturn =
        factory->NewSmiLiteral(JSGeneratorObject::RETURN, nopos);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, mode_proxy, kreturn, nopos);
    set_mode_return = factory->NewExpressionStatement(assignment, nopos);
  }


  // RawYield(output);
  Statement* yield_output;
  {
    Expression* output_proxy = factory->NewVariableProxy(var_output);
    Yield* yield = factory->NewYield(
        generator, output_proxy, Yield::kInitial, nopos);
    yield_output = factory->NewExpressionStatement(yield, nopos);
  }


  // mode = kNext;
  Statement* set_mode_next;
  {
    Expression* mode_proxy = factory->NewVariableProxy(var_mode);
    Expression* knext = factory->NewSmiLiteral(JSGeneratorObject::NEXT, nopos);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, mode_proxy, knext, nopos);
    set_mode_next = factory->NewExpressionStatement(assignment, nopos);
  }


  // mode = kThrow;
  Statement* set_mode_throw;
  {
    Expression* mode_proxy = factory->NewVariableProxy(var_mode);
    Expression* kthrow =
        factory->NewSmiLiteral(JSGeneratorObject::THROW, nopos);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, mode_proxy, kthrow, nopos);
    set_mode_throw = factory->NewExpressionStatement(assignment, nopos);
  }


  // input = function.sent;
  Statement* get_input;
  {
    Expression* function_sent = FunctionSentExpression(scope, factory, nopos);
    Expression* input_proxy = factory->NewVariableProxy(var_input);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, input_proxy, function_sent, nopos);
    get_input = factory->NewExpressionStatement(assignment, nopos);
  }


  // output.value;
  Statement* get_value;
  {
    Expression* output_proxy = factory->NewVariableProxy(var_output);
    Expression* literal =
        factory->NewStringLiteral(avfactory->value_string(), nopos);
    Expression* property = factory->NewProperty(output_proxy, literal, nopos);
    get_value = factory->NewExpressionStatement(property, nopos);
  }


  // Now put things together.


  // try { ... } catch(e) { ... }
  Statement* try_catch;
  {
    Block* try_block = factory->NewBlock(nullptr, 2, false, nopos);
    try_block->statements()->Add(yield_output, zone);
    try_block->statements()->Add(set_mode_next, zone);

    Block* catch_block = factory->NewBlock(nullptr, 1, false, nopos);
    catch_block->statements()->Add(set_mode_throw, zone);

    Scope* catch_scope = NewScope(scope, CATCH_SCOPE);
    const AstRawString* name = avfactory->dot_catch_string();
    Variable* catch_variable =
        catch_scope->DeclareLocal(name, VAR, kCreatedInitialized,
                                               Variable::NORMAL);

    try_catch = factory->NewTryCatchStatement(
        try_block, catch_scope, catch_variable, catch_block, nopos);
  }


  // try { ... } finally { ... }
  Statement* try_finally;
  {
    Block* try_block = factory->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(try_catch, zone);

    Block* finally = factory->NewBlock(nullptr, 2, false, nopos);
    finally->statements()->Add(get_input, zone);
    finally->statements()->Add(
        factory->NewContinueStatement(loop, nopos), zone);

    try_finally = factory->NewTryFinallyStatement(try_block, finally, nopos);
  }


  // switch (mode) { ... }
  SwitchStatement* switch_mode = factory->NewSwitchStatement(nullptr, nopos);
  {
    auto case_next = new (zone) ZoneList<Statement*>(3, zone);
    case_next->Add(call_next, zone);
    case_next->Add(validate_next_output, zone);
    case_next->Add(factory->NewBreakStatement(switch_mode, nopos), zone);

    auto case_return = new (zone) ZoneList<Statement*>(5, zone);
    BuildIteratorClose(case_return, var_iterator,
                       factory->NewVariableProxy(var_input, nopos), var_output);
    case_return->Add(factory->NewBreakStatement(switch_mode, nopos), zone);

    auto case_throw = new (zone) ZoneList<Statement*>(5, zone);
    case_throw->Add(get_throw, zone);
    case_throw->Add(check_throw, zone);
    case_throw->Add(call_throw, zone);
    case_throw->Add(validate_throw_output, zone);
    case_throw->Add(factory->NewBreakStatement(switch_mode, nopos), zone);

    auto cases = new (zone) ZoneList<CaseClause*>(3, zone);
    Expression* knext = factory->NewSmiLiteral(JSGeneratorObject::NEXT, nopos);
    Expression* kreturn =
        factory->NewSmiLiteral(JSGeneratorObject::RETURN, nopos);
    Expression* kthrow =
        factory->NewSmiLiteral(JSGeneratorObject::THROW, nopos);
    cases->Add(factory->NewCaseClause(knext, case_next, nopos), zone);
    cases->Add(factory->NewCaseClause(kreturn, case_return, nopos), zone);
    cases->Add(factory->NewCaseClause(kthrow, case_throw, nopos), zone);

    switch_mode->Initialize(factory->NewVariableProxy(var_mode), cases);
  }


  // while (true) { ... }
  // Already defined earlier: WhileStatement* loop = ...
  {
    Block* loop_body = factory->NewBlock(nullptr, 4, false, nopos);
    loop_body->statements()->Add(switch_mode, zone);
    loop_body->statements()->Add(if_done, zone);
    loop_body->statements()->Add(set_mode_return, zone);
    loop_body->statements()->Add(try_finally, zone);

    loop->Initialize(factory->NewBooleanLiteral(true, nopos), loop_body);
  }


  // do { ... }
  DoExpression* yield_star;
  {
    // The rewriter needs to process the get_value statement only, hence we
    // put the preceding statements into an init block.

    Block* do_block_ = factory->NewBlock(nullptr, 6, true, nopos);
    do_block_->statements()->Add(initialize_input, zone);
    do_block_->statements()->Add(initialize_mode, zone);
    do_block_->statements()->Add(initialize_output, zone);
    do_block_->statements()->Add(get_iterator, zone);
    do_block_->statements()->Add(validate_iterator, zone);
    do_block_->statements()->Add(loop, zone);

    Block* do_block = factory->NewBlock(nullptr, 2, false, nopos);
    do_block->statements()->Add(do_block_, zone);
    do_block->statements()->Add(get_value, zone);

    Variable* dot_result = scope->NewTemporary(avfactory->dot_result_string());
    yield_star = factory->NewDoExpression(do_block, dot_result, nopos);
    Rewriter::Rewrite(parser_, yield_star, avfactory);
  }

  return yield_star;
}

// Desugaring of (lhs) instanceof (rhs)
// ====================================
//
// We desugar instanceof into a load of property @@hasInstance on the rhs.
// We end up with roughly the following code (O, C):
//
//   do {
//     let O = lhs;
//     let C = rhs;
//     if (!IS_RECEIVER(C)) throw MakeTypeError(kNonObjectInInstanceOfCheck);
//     let handler_result = C[Symbol.hasInstance];
//     if (handler_result === undefined) {
//       if (!IS_CALLABLE(C)) {
//         throw MakeTypeError(kCalledNonCallableInstanceOf);
//       }
//       handler_result = %ordinary_has_instance(C, O);
//     } else {
//       handler_result = !!(%_Call(handler_result, C, O));
//     }
//     handler_result;
//   }
//
Expression* ParserTraits::RewriteInstanceof(Expression* lhs, Expression* rhs,
                                            int pos) {
  const int nopos = RelocInfo::kNoPosition;

  auto factory = parser_->factory();
  auto avfactory = parser_->ast_value_factory();
  auto scope = parser_->scope_;
  auto zone = parser_->zone();

  // let O = lhs;
  Variable* var_O = scope->NewTemporary(avfactory->empty_string());
  Statement* get_O;
  {
    Expression* O_proxy = factory->NewVariableProxy(var_O);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, O_proxy, lhs, nopos);
    get_O = factory->NewExpressionStatement(assignment, nopos);
  }

  // let C = lhs;
  Variable* var_C = scope->NewTemporary(avfactory->empty_string());
  Statement* get_C;
  {
    Expression* C_proxy = factory->NewVariableProxy(var_C);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, C_proxy, rhs, nopos);
    get_C = factory->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_RECEIVER(C)) throw MakeTypeError(kNonObjectInInstanceOfCheck);
  Statement* validate_C;
  {
    auto args = new (zone) ZoneList<Expression*>(1, zone);
    args->Add(factory->NewVariableProxy(var_C), zone);
    Expression* is_receiver_call =
        factory->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    Expression* call =
        NewThrowTypeError(MessageTemplate::kNonObjectInInstanceOfCheck,
                          avfactory->empty_string(), nopos);
    Statement* throw_call = factory->NewExpressionStatement(call, nopos);

    validate_C =
        factory->NewIfStatement(is_receiver_call,
                                factory->NewEmptyStatement(nopos),
                                throw_call,
                                nopos);
  }

  // let handler_result = C[Symbol.hasInstance];
  Variable* var_handler_result = scope->NewTemporary(avfactory->empty_string());
  Statement* initialize_handler;
  {
    Expression* hasInstance_symbol_literal =
        factory->NewSymbolLiteral("hasInstance_symbol", RelocInfo::kNoPosition);
    Expression* prop = factory->NewProperty(factory->NewVariableProxy(var_C),
                                            hasInstance_symbol_literal, pos);
    Expression* handler_proxy = factory->NewVariableProxy(var_handler_result);
    Expression* assignment =
        factory->NewAssignment(Token::ASSIGN, handler_proxy, prop, nopos);
    initialize_handler = factory->NewExpressionStatement(assignment, nopos);
  }

  // if (handler_result === undefined) {
  //   if (!IS_CALLABLE(C)) {
  //     throw MakeTypeError(kCalledNonCallableInstanceOf);
  //   }
  //   result = %ordinary_has_instance(C, O);
  // } else {
  //   handler_result = !!%_Call(handler_result, C, O);
  // }
  Statement* call_handler;
  {
    Expression* condition = factory->NewCompareOperation(
        Token::EQ_STRICT, factory->NewVariableProxy(var_handler_result),
        factory->NewUndefinedLiteral(nopos), nopos);

    Block* then_side = factory->NewBlock(nullptr, 2, false, nopos);
    {
      Expression* throw_expr =
          NewThrowTypeError(MessageTemplate::kCalledNonCallableInstanceOf,
                            avfactory->empty_string(), nopos);
      Statement* validate_C = CheckCallable(var_C, throw_expr);
      ZoneList<Expression*>* args = new (zone) ZoneList<Expression*>(2, zone);
      args->Add(factory->NewVariableProxy(var_C), zone);
      args->Add(factory->NewVariableProxy(var_O), zone);
      CallRuntime* call = factory->NewCallRuntime(
          Context::ORDINARY_HAS_INSTANCE_INDEX, args, pos);
      Expression* result_proxy = factory->NewVariableProxy(var_handler_result);
      Expression* assignment =
          factory->NewAssignment(Token::ASSIGN, result_proxy, call, nopos);
      Statement* assignment_return =
          factory->NewExpressionStatement(assignment, nopos);

      then_side->statements()->Add(validate_C, zone);
      then_side->statements()->Add(assignment_return, zone);
    }

    Statement* else_side;
    {
      auto args = new (zone) ZoneList<Expression*>(3, zone);
      args->Add(factory->NewVariableProxy(var_handler_result), zone);
      args->Add(factory->NewVariableProxy(var_C), zone);
      args->Add(factory->NewVariableProxy(var_O), zone);
      Expression* call =
          factory->NewCallRuntime(Runtime::kInlineCall, args, nopos);
      Expression* inner_not =
          factory->NewUnaryOperation(Token::NOT, call, nopos);
      Expression* outer_not =
          factory->NewUnaryOperation(Token::NOT, inner_not, nopos);
      Expression* result_proxy = factory->NewVariableProxy(var_handler_result);
      Expression* assignment =
          factory->NewAssignment(Token::ASSIGN, result_proxy, outer_not, nopos);

      else_side = factory->NewExpressionStatement(assignment, nopos);
    }
    call_handler =
        factory->NewIfStatement(condition, then_side, else_side, nopos);
  }

  // do { ... }
  DoExpression* instanceof;
  {
    Block* block = factory->NewBlock(nullptr, 5, true, nopos);
    block->statements()->Add(get_O, zone);
    block->statements()->Add(get_C, zone);
    block->statements()->Add(validate_C, zone);
    block->statements()->Add(initialize_handler, zone);
    block->statements()->Add(call_handler, zone);

    // Here is the desugared instanceof.
    instanceof = factory->NewDoExpression(block, var_handler_result, nopos);
    Rewriter::Rewrite(parser_, instanceof, avfactory);
  }

  return instanceof;
}

Statement* ParserTraits::CheckCallable(Variable* var, Expression* error) {
  auto factory = parser_->factory();
  auto avfactory = parser_->ast_value_factory();
  const int nopos = RelocInfo::kNoPosition;
  Statement* validate_var;
  {
    Expression* type_of = factory->NewUnaryOperation(
        Token::TYPEOF, factory->NewVariableProxy(var), nopos);
    Expression* function_literal =
        factory->NewStringLiteral(avfactory->function_string(), nopos);
    Expression* condition = factory->NewCompareOperation(
        Token::EQ_STRICT, type_of, function_literal, nopos);

    Statement* throw_call = factory->NewExpressionStatement(error, nopos);

    validate_var = factory->NewIfStatement(
        condition, factory->NewEmptyStatement(nopos), throw_call, nopos);
  }
  return validate_var;
}

void ParserTraits::BuildIteratorClose(ZoneList<Statement*>* statements,
                                      Variable* iterator,
                                      Expression* input,
                                      Variable* var_output) {
  //
  // This function adds four statements to [statements], corresponding to the
  // following code:
  //
  //   let iteratorReturn = iterator.return;
  //   if (IS_NULL_OR_UNDEFINED(iteratorReturn) return input;
  //   output = %_Call(iteratorReturn, iterator);
  //   if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
  //

  const int nopos = RelocInfo::kNoPosition;
  auto factory = parser_->factory();
  auto avfactory = parser_->ast_value_factory();
  auto zone = parser_->zone();

  // let iteratorReturn = iterator.return;
  Variable* var_return = var_output;  // Reusing the output variable.
  Statement* get_return;
  {
    Expression* iterator_proxy = factory->NewVariableProxy(iterator);
    Expression* literal =
        factory->NewStringLiteral(avfactory->return_string(), nopos);
    Expression* property =
        factory->NewProperty(iterator_proxy, literal, nopos);
    Expression* return_proxy = factory->NewVariableProxy(var_return);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, return_proxy, property, nopos);
    get_return = factory->NewExpressionStatement(assignment, nopos);
  }

  // if (IS_NULL_OR_UNDEFINED(iteratorReturn) return input;
  Statement* check_return;
  {
    Expression* condition = factory->NewCompareOperation(
        Token::EQ, factory->NewVariableProxy(var_return),
        factory->NewNullLiteral(nopos), nopos);

    Statement* return_input = factory->NewReturnStatement(input, nopos);

    check_return = factory->NewIfStatement(
        condition, return_input, factory->NewEmptyStatement(nopos), nopos);
  }

  // output = %_Call(iteratorReturn, iterator);
  Statement* call_return;
  {
    auto args = new (zone) ZoneList<Expression*>(3, zone);
    args->Add(factory->NewVariableProxy(var_return), zone);
    args->Add(factory->NewVariableProxy(iterator), zone);

    Expression* call =
        factory->NewCallRuntime(Runtime::kInlineCall, args, nopos);
    Expression* output_proxy = factory->NewVariableProxy(var_output);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, output_proxy, call, nopos);
    call_return = factory->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_RECEIVER(output)) %ThrowIteratorResultNotAnObject(output);
  Statement* validate_output;
  {
    Expression* is_receiver_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      is_receiver_call =
          factory->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      Expression* call = factory->NewCallRuntime(
          Runtime::kThrowIteratorResultNotAnObject, args, nopos);
      throw_call = factory->NewExpressionStatement(call, nopos);
    }

    validate_output = factory->NewIfStatement(
        is_receiver_call, factory->NewEmptyStatement(nopos), throw_call, nopos);
  }

  statements->Add(get_return, zone);
  statements->Add(check_return, zone);
  statements->Add(call_return, zone);
  statements->Add(validate_output, zone);
}


// Runtime encoding of different completion modes.
enum ForOfLoopBodyCompletion { BODY_COMPLETED, BODY_ABORTED, BODY_THREW };

void ParserTraits::BuildIteratorCloseForCompletion(
    ZoneList<Statement*>* statements, Variable* iterator,
    Variable* completion) {
  //
  // This function adds two statements to [statements], corresponding to the
  // following code:
  //
  //   let iteratorReturn = iterator.return;
  //   if (!IS_NULL_OR_UNDEFINED(iteratorReturn)) {
  //     let output;
  //     if (completion === BODY_THREW) {
  //       if (!IS_CALLABLE(iteratorReturn)) {
  //         throw MakeTypeError(kReturnMethodNotCallable);
  //       }
  //       try { output = %_Call(iteratorReturn, iterator) } catch (_) { }
  //     } else {
  //       output = %_Call(iteratorReturn, iterator);
  //     }
  //     if (!IS_RECEIVER(output)) %ThrowIterResultNotAnObject(output);
  //   }
  //

  const int nopos = RelocInfo::kNoPosition;
  auto factory = parser_->factory();
  auto avfactory = parser_->ast_value_factory();
  auto scope = parser_->scope_;
  auto zone = parser_->zone();

  // let output;
  Variable* var_output = scope->NewTemporary(avfactory->empty_string());

  // let iteratorReturn = iterator.return;
  Variable* var_return = var_output;  // Reusing the output variable.
  Statement* get_return;
  {
    Expression* iterator_proxy = factory->NewVariableProxy(iterator);
    Expression* literal =
        factory->NewStringLiteral(avfactory->return_string(), nopos);
    Expression* property =
        factory->NewProperty(iterator_proxy, literal, nopos);
    Expression* return_proxy = factory->NewVariableProxy(var_return);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, return_proxy, property, nopos);
    get_return = factory->NewExpressionStatement(assignment, nopos);
  }

  // if (!IS_CALLABLE(iteratorReturn)) {
  //   throw MakeTypeError(kReturnMethodNotCallable);
  // }
  Statement* check_return_callable;
  {
    Expression* throw_expr = NewThrowTypeError(
        MessageTemplate::kReturnMethodNotCallable,
        avfactory->empty_string(), nopos);
    check_return_callable = CheckCallable(var_return, throw_expr);
  }

  // output = %_Call(iteratorReturn, iterator);
  Statement* call_return;
  {
    auto args = new (zone) ZoneList<Expression*>(2, zone);
    args->Add(factory->NewVariableProxy(var_return), zone);
    args->Add(factory->NewVariableProxy(iterator), zone);
    Expression* call =
        factory->NewCallRuntime(Runtime::kInlineCall, args, nopos);

    Expression* output_proxy = factory->NewVariableProxy(var_output);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, output_proxy, call, nopos);
    call_return = factory->NewExpressionStatement(assignment, nopos);
  }

  // try { output = %_Call(iteratorReturn, iterator) } catch (_) { }
  Statement* try_call_return;
  {
    auto args = new (zone) ZoneList<Expression*>(2, zone);
    args->Add(factory->NewVariableProxy(var_return), zone);
    args->Add(factory->NewVariableProxy(iterator), zone);

    Expression* call =
        factory->NewCallRuntime(Runtime::kInlineCall, args, nopos);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, factory->NewVariableProxy(var_output), call, nopos);

    Block* try_block = factory->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(
        factory->NewExpressionStatement(assignment, nopos), zone);

    Block* catch_block = factory->NewBlock(nullptr, 0, false, nopos);

    Scope* catch_scope = NewScope(scope, CATCH_SCOPE);
    Variable* catch_variable = catch_scope->DeclareLocal(
        avfactory->dot_catch_string(), VAR, kCreatedInitialized,
        Variable::NORMAL);

    try_call_return = factory->NewTryCatchStatement(
        try_block, catch_scope, catch_variable, catch_block, nopos);
  }

  // if (completion === ABRUPT_THROW) {
  //   #check_return_callable;
  //   #try_call_return;
  // } else {
  //   #call_return;
  // }
  Statement* call_return_carefully;
  {
    Expression* condition = factory->NewCompareOperation(
        Token::EQ_STRICT, factory->NewVariableProxy(completion),
        factory->NewSmiLiteral(BODY_THREW, nopos), nopos);

    Block* then_block = factory->NewBlock(nullptr, 2, false, nopos);
    then_block->statements()->Add(check_return_callable, zone);
    then_block->statements()->Add(try_call_return, zone);

    call_return_carefully =
        factory->NewIfStatement(condition, then_block, call_return, nopos);
  }

  // if (!IS_RECEIVER(output)) %ThrowIteratorResultNotAnObject(output);
  Statement* validate_output;
  {
    Expression* is_receiver_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      is_receiver_call =
          factory->NewCallRuntime(Runtime::kInlineIsJSReceiver, args, nopos);
    }

    Statement* throw_call;
    {
      auto args = new (zone) ZoneList<Expression*>(1, zone);
      args->Add(factory->NewVariableProxy(var_output), zone);
      Expression* call = factory->NewCallRuntime(
          Runtime::kThrowIteratorResultNotAnObject, args, nopos);
      throw_call = factory->NewExpressionStatement(call, nopos);
    }

    validate_output = factory->NewIfStatement(
        is_receiver_call, factory->NewEmptyStatement(nopos), throw_call, nopos);
  }

  // if (!IS_NULL_OR_UNDEFINED(iteratorReturn)) { ... }
  Statement* maybe_call_return;
  {
    Expression* condition = factory->NewCompareOperation(
        Token::EQ, factory->NewVariableProxy(var_return),
        factory->NewNullLiteral(nopos), nopos);

    Block* block = factory->NewBlock(nullptr, 2, false, nopos);
    block->statements()->Add(call_return_carefully, zone);
    block->statements()->Add(validate_output, zone);

    maybe_call_return = factory->NewIfStatement(
        condition, factory->NewEmptyStatement(nopos), block, nopos);
  }


  statements->Add(get_return, zone);
  statements->Add(maybe_call_return, zone);
}


Statement* ParserTraits::FinalizeForOfStatement(ForOfStatement* loop, int pos) {
  if (!FLAG_harmony_iterator_close) return loop;

  //
  // This function replaces the loop with the following wrapping:
  //
  //   let completion = BODY_COMPLETED;
  //   try {
  //     #loop;
  //   } catch(e) {
  //     if (completion === BODY_ABORTED) completion = BODY_THREW;
  //     throw e;
  //   } finally {
  //     if (!(completion === BODY_COMPLETED || IS_UNDEFINED(#iterator))) {
  //       #BuildIteratorClose(#iterator, completion)  // See above.
  //     }
  //   }
  //
  // where the loop's body is wrapped as follows:
  //
  //   {
  //     {{completion = BODY_ABORTED;}}
  //     #loop-body
  //     {{completion = BODY_COMPLETED;}}
  //   }

  const int nopos = RelocInfo::kNoPosition;
  auto factory = parser_->factory();
  auto avfactory = parser_->ast_value_factory();
  auto scope = parser_->scope_;
  auto zone = parser_->zone();

  // let completion = BODY_COMPLETED;
  Variable* var_completion = scope->NewTemporary(avfactory->empty_string());
  Statement* initialize_completion;
  {
    Expression* proxy = factory->NewVariableProxy(var_completion);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, proxy,
        factory->NewSmiLiteral(BODY_COMPLETED, nopos), nopos);
    initialize_completion =
        factory->NewExpressionStatement(assignment, nopos);
  }

  // if (completion === BODY_ABORTED) completion = BODY_THREW;
  Statement* set_completion_throw;
  {
    Expression* condition = factory->NewCompareOperation(
        Token::EQ_STRICT, factory->NewVariableProxy(var_completion),
        factory->NewSmiLiteral(BODY_ABORTED, nopos), nopos);

    Expression* proxy = factory->NewVariableProxy(var_completion);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, proxy, factory->NewSmiLiteral(BODY_THREW, nopos),
        nopos);
    Statement* statement = factory->NewExpressionStatement(assignment, nopos);
    set_completion_throw = factory->NewIfStatement(
        condition, statement, factory->NewEmptyStatement(nopos), nopos);
  }

  // if (!(completion === BODY_COMPLETED || IS_UNDEFINED(#iterator))) {
  //   #BuildIteratorClose(#iterator, completion)
  // }
  Block* maybe_close;
  {
    Expression* condition1 = factory->NewCompareOperation(
        Token::EQ_STRICT, factory->NewVariableProxy(var_completion),
        factory->NewSmiLiteral(BODY_COMPLETED, nopos), nopos);
    Expression* condition2 = factory->NewCompareOperation(
        Token::EQ_STRICT, factory->NewVariableProxy(loop->iterator()),
        factory->NewUndefinedLiteral(nopos), nopos);
    Expression* condition = factory->NewBinaryOperation(
        Token::OR, condition1, condition2, nopos);

    Block* block = factory->NewBlock(nullptr, 2, false, nopos);
    BuildIteratorCloseForCompletion(
        block->statements(), loop->iterator(), var_completion);
    DCHECK(block->statements()->length() == 2);

    maybe_close = factory->NewBlock(nullptr, 1, false, nopos);
    maybe_close->statements()->Add(factory->NewIfStatement(
        condition, factory->NewEmptyStatement(nopos), block, nopos), zone);
  }

  // try { #try_block }
  // catch(e) {
  //   #set_completion_throw;
  //   throw e;
  // }
  Statement* try_catch;
  {
    Scope* catch_scope = NewScope(scope, CATCH_SCOPE);
    Variable* catch_variable = catch_scope->DeclareLocal(
        avfactory->dot_catch_string(), VAR, kCreatedInitialized,
        Variable::NORMAL);

    Statement* rethrow;
    {
      Expression* proxy = factory->NewVariableProxy(catch_variable);
      rethrow = factory->NewExpressionStatement(
          factory->NewThrow(proxy, nopos), nopos);
    }

    Block* try_block = factory->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(loop, zone);

    Block* catch_block = factory->NewBlock(nullptr, 2, false, nopos);
    catch_block->statements()->Add(set_completion_throw, zone);
    catch_block->statements()->Add(rethrow, zone);

    try_catch = factory->NewTryCatchStatement(
        try_block, catch_scope, catch_variable, catch_block, nopos);
  }

  // try { #try_catch } finally { #maybe_close }
  Statement* try_finally;
  {
    Block* try_block = factory->NewBlock(nullptr, 1, false, nopos);
    try_block->statements()->Add(try_catch, zone);

    try_finally =
        factory->NewTryFinallyStatement(try_block, maybe_close, nopos);
  }

  // #initialize_completion;
  // #try_finally;
  Statement* final_loop;
  {
    Block* block = factory->NewBlock(nullptr, 2, false, nopos);
    block->statements()->Add(initialize_completion, zone);
    block->statements()->Add(try_finally, zone);
    final_loop = block;
  }

  // {{completion = BODY_ABORTED;}}
  Statement* set_completion_break;
  {
    Expression* proxy = factory->NewVariableProxy(var_completion);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, proxy,
        factory->NewSmiLiteral(BODY_ABORTED, nopos), nopos);

    Block* block = factory->NewBlock(nullptr, 1, true, nopos);
    block->statements()->Add(
        factory->NewExpressionStatement(assignment, nopos), zone);
    set_completion_break = block;
  }

  // {{completion = BODY_COMPLETED;}}
  Statement* set_completion_normal;
  {
    Expression* proxy = factory->NewVariableProxy(var_completion);
    Expression* assignment = factory->NewAssignment(
        Token::ASSIGN, proxy, factory->NewSmiLiteral(BODY_COMPLETED, nopos),
        nopos);

    Block* block = factory->NewBlock(nullptr, 1, true, nopos);
    block->statements()->Add(
        factory->NewExpressionStatement(assignment, nopos), zone);
    set_completion_normal = block;
  }

  // { #set_completion_break; #loop-body; #set_completion_normal }
  Block* new_body = factory->NewBlock(nullptr, 2, false, nopos);
  new_body->statements()->Add(set_completion_break, zone);
  new_body->statements()->Add(loop->body(), zone);
  new_body->statements()->Add(set_completion_normal, zone);

  loop->set_body(new_body);
  return final_loop;
}


}  // namespace internal
}  // namespace v8
