// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/api.h"
#include "src/ast.h"
#include "src/bailout-reason.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/char-predicates-inl.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/messages.h"
#include "src/parser.h"
#include "src/preparser.h"
#include "src/runtime/runtime.h"
#include "src/scanner-character-streams.h"
#include "src/scopeinfo.h"
#include "src/string-stream.h"

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
  if (!script.is_null() && script->type()->value() == Script::TYPE_NATIVE) {
    set_native();
  }
}


ParseInfo::ParseInfo(Zone* zone, Handle<Script> script) : ParseInfo(zone) {
  isolate_ = script->GetIsolate();

  set_hash_seed(isolate_->heap()->HashSeed());
  set_stack_limit(isolate_->stack_guard()->real_climit());
  set_unicode_cache(isolate_->unicode_cache());
  set_script(script);

  if (script->type()->value() == Script::TYPE_NATIVE) {
    set_native();
  }
}


RegExpBuilder::RegExpBuilder(Zone* zone)
    : zone_(zone),
      pending_empty_(false),
      characters_(NULL),
      terms_(),
      alternatives_()
#ifdef DEBUG
    , last_added_(ADD_NONE)
#endif
  {}


void RegExpBuilder::FlushCharacters() {
  pending_empty_ = false;
  if (characters_ != NULL) {
    RegExpTree* atom = new(zone()) RegExpAtom(characters_->ToConstVector());
    characters_ = NULL;
    text_.Add(atom, zone());
    LAST(ADD_ATOM);
  }
}


void RegExpBuilder::FlushText() {
  FlushCharacters();
  int num_text = text_.length();
  if (num_text == 0) {
    return;
  } else if (num_text == 1) {
    terms_.Add(text_.last(), zone());
  } else {
    RegExpText* text = new(zone()) RegExpText(zone());
    for (int i = 0; i < num_text; i++)
      text_.Get(i)->AppendToText(text, zone());
    terms_.Add(text, zone());
  }
  text_.Clear();
}


void RegExpBuilder::AddCharacter(uc16 c) {
  pending_empty_ = false;
  if (characters_ == NULL) {
    characters_ = new(zone()) ZoneList<uc16>(4, zone());
  }
  characters_->Add(c, zone());
  LAST(ADD_CHAR);
}


void RegExpBuilder::AddEmpty() {
  pending_empty_ = true;
}


void RegExpBuilder::AddAtom(RegExpTree* term) {
  if (term->IsEmpty()) {
    AddEmpty();
    return;
  }
  if (term->IsTextElement()) {
    FlushCharacters();
    text_.Add(term, zone());
  } else {
    FlushText();
    terms_.Add(term, zone());
  }
  LAST(ADD_ATOM);
}


void RegExpBuilder::AddAssertion(RegExpTree* assert) {
  FlushText();
  terms_.Add(assert, zone());
  LAST(ADD_ASSERT);
}


void RegExpBuilder::NewAlternative() {
  FlushTerms();
}


void RegExpBuilder::FlushTerms() {
  FlushText();
  int num_terms = terms_.length();
  RegExpTree* alternative;
  if (num_terms == 0) {
    alternative = new (zone()) RegExpEmpty();
  } else if (num_terms == 1) {
    alternative = terms_.last();
  } else {
    alternative = new(zone()) RegExpAlternative(terms_.GetList(zone()));
  }
  alternatives_.Add(alternative, zone());
  terms_.Clear();
  LAST(ADD_NONE);
}


RegExpTree* RegExpBuilder::ToRegExp() {
  FlushTerms();
  int num_alternatives = alternatives_.length();
  if (num_alternatives == 0) return new (zone()) RegExpEmpty();
  if (num_alternatives == 1) return alternatives_.last();
  return new(zone()) RegExpDisjunction(alternatives_.GetList(zone()));
}


void RegExpBuilder::AddQuantifierToAtom(
    int min, int max, RegExpQuantifier::QuantifierType quantifier_type) {
  if (pending_empty_) {
    pending_empty_ = false;
    return;
  }
  RegExpTree* atom;
  if (characters_ != NULL) {
    DCHECK(last_added_ == ADD_CHAR);
    // Last atom was character.
    Vector<const uc16> char_vector = characters_->ToConstVector();
    int num_chars = char_vector.length();
    if (num_chars > 1) {
      Vector<const uc16> prefix = char_vector.SubVector(0, num_chars - 1);
      text_.Add(new(zone()) RegExpAtom(prefix), zone());
      char_vector = char_vector.SubVector(num_chars - 1, num_chars);
    }
    characters_ = NULL;
    atom = new(zone()) RegExpAtom(char_vector);
    FlushText();
  } else if (text_.length() > 0) {
    DCHECK(last_added_ == ADD_ATOM);
    atom = text_.RemoveLast();
    FlushText();
  } else if (terms_.length() > 0) {
    DCHECK(last_added_ == ADD_ATOM);
    atom = terms_.RemoveLast();
    if (atom->max_match() == 0) {
      // Guaranteed to only match an empty string.
      LAST(ADD_TERM);
      if (min == 0) {
        return;
      }
      terms_.Add(atom, zone());
      return;
    }
  } else {
    // Only call immediately after adding an atom or character!
    UNREACHABLE();
    return;
  }
  terms_.Add(
      new(zone()) RegExpQuantifier(min, max, quantifier_type, atom), zone());
  LAST(ADD_TERM);
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


FunctionLiteral* Parser::DefaultConstructor(bool call_super, Scope* scope,
                                            int pos, int end_pos) {
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  int handler_count = 0;
  int parameter_count = 0;
  const AstRawString* name = ast_value_factory()->empty_string();


  FunctionKind kind = call_super ? FunctionKind::kDefaultSubclassConstructor
                                 : FunctionKind::kDefaultBaseConstructor;
  Scope* function_scope = NewScope(scope, FUNCTION_SCOPE, kind);
  function_scope->SetLanguageMode(
      static_cast<LanguageMode>(scope->language_mode() | STRICT_BIT));
  // Set start and end position to the same value
  function_scope->set_start_position(pos);
  function_scope->set_end_position(pos);
  ZoneList<Statement*>* body = NULL;

  {
    AstNodeFactory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, function_scope,
                                 kind, &function_factory);

    body = new (zone()) ZoneList<Statement*>(call_super ? 2 : 1, zone());
    AddAssertIsConstruct(body, pos);
    if (call_super) {
      ZoneList<Expression*>* args =
          new (zone()) ZoneList<Expression*>(0, zone());
      CallRuntime* call = factory()->NewCallRuntime(
          ast_value_factory()->empty_string(),
          Runtime::FunctionForId(Runtime::kInlineDefaultConstructorCallSuper),
          args, pos);
      body->Add(factory()->NewReturnStatement(call, pos), zone());
    }

    materialized_literal_count = function_state.materialized_literal_count();
    expected_property_count = function_state.expected_property_count();
    handler_count = function_state.handler_count();
  }

  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      name, ast_value_factory(), function_scope, body,
      materialized_literal_count, expected_property_count, handler_count,
      parameter_count, FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::ANONYMOUS_EXPRESSION, FunctionLiteral::kIsFunction,
      FunctionLiteral::kNotParenthesized, kind, pos);

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


void ParserTraits::CheckPossibleEvalCall(Expression* expression,
                                         Scope* scope) {
  VariableProxy* callee = expression->AsVariableProxy();
  if (callee != NULL &&
      callee->raw_name() == parser_->ast_value_factory()->eval_string()) {
    scope->DeclarationScope()->RecordEvalCall();
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
    switch (op) {
      case Token::ADD:
        *x = factory->NewNumberLiteral(x_val + y_val, pos);
        return true;
      case Token::SUB:
        *x = factory->NewNumberLiteral(x_val - y_val, pos);
        return true;
      case Token::MUL:
        *x = factory->NewNumberLiteral(x_val * y_val, pos);
        return true;
      case Token::DIV:
        *x = factory->NewNumberLiteral(x_val / y_val, pos);
        return true;
      case Token::BIT_OR: {
        int value = DoubleToInt32(x_val) | DoubleToInt32(y_val);
        *x = factory->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::BIT_AND: {
        int value = DoubleToInt32(x_val) & DoubleToInt32(y_val);
        *x = factory->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::BIT_XOR: {
        int value = DoubleToInt32(x_val) ^ DoubleToInt32(y_val);
        *x = factory->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SHL: {
        int value = DoubleToInt32(x_val) << (DoubleToInt32(y_val) & 0x1f);
        *x = factory->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SHR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1f;
        uint32_t value = DoubleToUint32(x_val) >> shift;
        *x = factory->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SAR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1f;
        int value = ArithmeticShiftRight(DoubleToInt32(x_val), shift);
        *x = factory->NewNumberLiteral(value, pos);
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
      switch (op) {
        case Token::ADD:
          return expression;
        case Token::SUB:
          return factory->NewNumberLiteral(-value, pos);
        case Token::BIT_NOT:
          return factory->NewNumberLiteral(~DoubleToInt32(value), pos);
        default:
          break;
      }
    }
  }
  // Desugar '+foo' => 'foo*1'
  if (op == Token::ADD) {
    return factory->NewBinaryOperation(
        Token::MUL, expression, factory->NewNumberLiteral(1, pos), pos);
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


Expression* ParserTraits::NewThrowReferenceError(const char* message, int pos) {
  return NewThrowError(
      parser_->ast_value_factory()->make_reference_error_string(), message,
      parser_->ast_value_factory()->empty_string(), pos);
}


Expression* ParserTraits::NewThrowSyntaxError(
    const char* message, const AstRawString* arg, int pos) {
  return NewThrowError(parser_->ast_value_factory()->make_syntax_error_string(),
                       message, arg, pos);
}


Expression* ParserTraits::NewThrowTypeError(
    const char* message, const AstRawString* arg, int pos) {
  return NewThrowError(parser_->ast_value_factory()->make_type_error_string(),
                       message, arg, pos);
}


Expression* ParserTraits::NewThrowError(
    const AstRawString* constructor, const char* message,
    const AstRawString* arg, int pos) {
  Zone* zone = parser_->zone();
  const AstRawString* type =
      parser_->ast_value_factory()->GetOneByteString(message);
  ZoneList<Expression*>* args = new (zone) ZoneList<Expression*>(2, zone);
  args->Add(parser_->factory()->NewStringLiteral(type, pos), zone);
  args->Add(parser_->factory()->NewStringLiteral(arg, pos), zone);
  CallRuntime* call_constructor =
      parser_->factory()->NewCallRuntime(constructor, NULL, args, pos);
  return parser_->factory()->NewThrow(call_constructor, pos);
}


void ParserTraits::ReportMessageAt(Scanner::Location source_location,
                                   const char* message, const char* arg,
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


void ParserTraits::ReportMessage(const char* message, const char* arg,
                                 ParseErrorType error_type) {
  Scanner::Location source_location = parser_->scanner()->location();
  ReportMessageAt(source_location, message, arg, error_type);
}


void ParserTraits::ReportMessage(const char* message, const AstRawString* arg,
                                 ParseErrorType error_type) {
  Scanner::Location source_location = parser_->scanner()->location();
  ReportMessageAt(source_location, message, arg, error_type);
}


void ParserTraits::ReportMessageAt(Scanner::Location source_location,
                                   const char* message, const AstRawString* arg,
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
  return factory->NewVariableProxy(scope->receiver(), pos);
}

Expression* ParserTraits::SuperReference(Scope* scope, AstNodeFactory* factory,
                                         int pos) {
  return factory->NewSuperReference(
      ThisExpression(scope, factory, pos)->AsVariableProxy(),
      pos);
}


Expression* ParserTraits::DefaultConstructor(bool call_super, Scope* scope,
                                             int pos, int end_pos) {
  return parser_->DefaultConstructor(call_super, scope, pos, end_pos);
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
      double value = scanner->DoubleValue();
      return factory->NewNumberLiteral(value, pos);
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

  // Arrow function parameters are parsed as an expression. When
  // parsing lazily, it is enough to create a VariableProxy in order
  // for Traits::DeclareArrowParametersFromExpression() to be able to
  // pick the names of the parameters.
  return parser_->parsing_lazy_arrow_parameters_
             ? factory->NewVariableProxy(name, Variable::NORMAL, start_position,
                                         end_position)
             : scope->NewUnresolved(factory, name, start_position,
                                    end_position);
}


Expression* ParserTraits::ExpressionFromString(int pos, Scanner* scanner,
                                               AstNodeFactory* factory) {
  const AstRawString* symbol = GetSymbol(scanner);
  if (parser_->fni_ != NULL) parser_->fni_->PushLiteralName(symbol);
  return factory->NewStringLiteral(symbol, pos);
}


Expression* ParserTraits::GetIterator(Expression* iterable,
                                      AstNodeFactory* factory) {
  Expression* iterator_symbol_literal =
      factory->NewSymbolLiteral("iterator_symbol", RelocInfo::kNoPosition);
  int pos = iterable->position();
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
    bool name_is_strict_reserved, FunctionKind kind,
    int function_token_position, FunctionLiteral::FunctionType type,
    FunctionLiteral::ArityRestriction arity_restriction, bool* ok) {
  return parser_->ParseFunctionLiteral(
      name, function_name_location, name_is_strict_reserved, kind,
      function_token_position, type, arity_restriction, ok);
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
      parsing_lazy_arrow_parameters_(false),
      total_preparse_skipped_(0),
      pre_parse_timer_(NULL),
      parsing_on_main_thread_(true) {
  // Even though we were passed ParseInfo, we should not store it in
  // Parser - this makes sure that Isolate is not accidentally accessed via
  // ParseInfo during background parsing.
  DCHECK(!info->script().is_null() || info->source_stream() != NULL);
  set_allow_lazy(info->allow_lazy_parsing());
  set_allow_natives(FLAG_allow_natives_syntax || info->is_native());
  set_allow_harmony_modules(!info->is_native() && FLAG_harmony_modules);
  set_allow_harmony_arrow_functions(FLAG_harmony_arrow_functions);
  set_allow_harmony_numeric_literals(FLAG_harmony_numeric_literals);
  set_allow_harmony_classes(FLAG_harmony_classes);
  set_allow_harmony_object_literals(FLAG_harmony_object_literals);
  set_allow_harmony_sloppy(FLAG_harmony_sloppy);
  set_allow_harmony_unicode(FLAG_harmony_unicode);
  set_allow_harmony_computed_property_names(
      FLAG_harmony_computed_property_names);
  set_allow_harmony_rest_params(FLAG_harmony_rest_parameters);
  set_allow_strong_mode(FLAG_strong_mode);
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

  Scope* top_scope = NULL;
  Scope* eval_scope = NULL;
  if (source->IsExternalTwoByteString()) {
    // Notice that the stream is destroyed at the end of the branch block.
    // The last line of the blocks can't be moved outside, even though they're
    // identical calls.
    ExternalTwoByteStringUtf16CharacterStream stream(
        Handle<ExternalTwoByteString>::cast(source), 0, source->length());
    scanner_.Initialize(&stream);
    result = DoParseProgram(info, &top_scope, &eval_scope);
  } else {
    GenericStringUtf16CharacterStream stream(source, 0, source->length());
    scanner_.Initialize(&stream);
    result = DoParseProgram(info, &top_scope, &eval_scope);
  }
  top_scope->set_end_position(source->length());
  if (eval_scope != NULL) {
    eval_scope->set_end_position(source->length());
  }
  HandleSourceURLComments(isolate, info->script());

  if (FLAG_trace_parse && result != NULL) {
    double ms = timer.Elapsed().InMillisecondsF();
    if (info->is_eval()) {
      PrintF("[parsing eval");
    } else if (info->script()->name()->IsString()) {
      String* name = String::cast(info->script()->name());
      SmartArrayPointer<char> name_chars = name->ToCString();
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


FunctionLiteral* Parser::DoParseProgram(ParseInfo* info, Scope** scope,
                                        Scope** eval_scope) {
  // Note that this function can be called from the main thread or from a
  // background thread. We should not access anything Isolate / heap dependent
  // via ParseInfo, and also not pass it forward.
  DCHECK(scope_ == NULL);
  DCHECK(target_stack_ == NULL);

  FunctionLiteral* result = NULL;
  {
    *scope = NewScope(scope_, SCRIPT_SCOPE);
    info->set_script_scope(*scope);
    if (!info->context().is_null() && !info->context()->IsNativeContext()) {
      *scope = Scope::DeserializeScopeChain(info->isolate(), zone(),
                                            *info->context(), *scope);
      // The Scope is backed up by ScopeInfo (which is in the V8 heap); this
      // means the Parser cannot operate independent of the V8 heap. Tell the
      // string table to internalize strings and values right after they're
      // created. This kind of parsing can only be done in the main thread.
      DCHECK(parsing_on_main_thread_);
      ast_value_factory()->Internalize(info->isolate());
    }
    original_scope_ = *scope;
    if (info->is_eval()) {
      if (!(*scope)->is_script_scope() || is_strict(info->language_mode())) {
        *scope = NewScope(*scope, EVAL_SCOPE);
      }
    } else if (info->is_module()) {
      *scope = NewScope(*scope, MODULE_SCOPE);
    }
    (*scope)->set_start_position(0);
    // End position will be set by the caller.

    // Compute the parsing mode.
    Mode mode = (FLAG_lazy && allow_lazy()) ? PARSE_LAZILY : PARSE_EAGERLY;
    if (allow_natives() || extension_ != NULL ||
        (*scope)->is_eval_scope()) {
      mode = PARSE_EAGERLY;
    }
    ParsingModeScope parsing_mode(this, mode);

    // Enters 'scope'.
    AstNodeFactory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, *scope,
                                 kNormalFunction, &function_factory);

    scope_->SetLanguageMode(info->language_mode());
    ZoneList<Statement*>* body = new(zone()) ZoneList<Statement*>(16, zone());
    bool ok = true;
    int beg_pos = scanner()->location().beg_pos;
    if (info->is_module()) {
      DCHECK(allow_harmony_modules());
      ParseModuleItemList(body, &ok);
    } else {
      ParseStatementList(body, Token::EOS, info->is_eval(), eval_scope, &ok);
    }

    if (ok && is_strict(language_mode())) {
      CheckStrictOctalLiteral(beg_pos, scanner()->location().end_pos, &ok);
      CheckConflictingVarDeclarations(scope_, &ok);
    }

    if (ok && info->parse_restriction() == ONLY_SINGLE_FUNCTION_LITERAL) {
      if (body->length() != 1 ||
          !body->at(0)->IsExpressionStatement() ||
          !body->at(0)->AsExpressionStatement()->
              expression()->IsFunctionLiteral()) {
        ReportMessage("single_function_literal");
        ok = false;
      }
    }

    if (ok) {
      result = factory()->NewFunctionLiteral(
          ast_value_factory()->empty_string(), ast_value_factory(), scope_,
          body, function_state.materialized_literal_count(),
          function_state.expected_property_count(),
          function_state.handler_count(), 0,
          FunctionLiteral::kNoDuplicateParameters,
          FunctionLiteral::ANONYMOUS_EXPRESSION, FunctionLiteral::kGlobalOrEval,
          FunctionLiteral::kNotParenthesized, FunctionKind::kNormalFunction, 0);
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
    SmartArrayPointer<char> name_chars = result->debug_name()->ToCString();
    PrintF("[parsing function: %s - took %0.3f ms]\n", name_chars.get(), ms);
  }
  return result;
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
    scope->SetLanguageMode(shared_info->language_mode());
    FunctionLiteral::FunctionType function_type = shared_info->is_expression()
        ? (shared_info->is_anonymous()
              ? FunctionLiteral::ANONYMOUS_EXPRESSION
              : FunctionLiteral::NAMED_EXPRESSION)
        : FunctionLiteral::DECLARATION;
    bool ok = true;

    if (shared_info->is_arrow()) {
      // The first expression being parsed is the parameter list of the arrow
      // function. Setting this avoids prevents ExpressionFromIdentifier()
      // from creating unresolved variables in already-resolved scopes.
      parsing_lazy_arrow_parameters_ = true;
      Expression* expression = ParseExpression(false, &ok);
      if (ok) {
        // Scanning must end at the same position that was recorded
        // previously. If not, parsing has been interrupted due to a
        // stack overflow, at which point the partially parsed arrow
        // function concise body happens to be a valid expression. This
        // is a problem only for arrow functions with single statement
        // bodies, since there is no end token such as "}" for normal
        // functions.
        if (scanner()->location().end_pos == shared_info->end_position()) {
          // The pre-parser saw an arrow function here, so the full parser
          // must produce a FunctionLiteral.
          DCHECK(expression->IsFunctionLiteral());
          result = expression->AsFunctionLiteral();
        } else {
          result = NULL;
          ok = false;
        }
      }
    } else if (shared_info->is_default_constructor()) {
      result = DefaultConstructor(IsSubclassConstructor(shared_info->kind()),
                                  scope, shared_info->start_position(),
                                  shared_info->end_position());
    } else {
      result = ParseFunctionLiteral(raw_name, Scanner::Location::invalid(),
                                    false,  // Strict mode name already checked.
                                    shared_info->kind(), RelocInfo::kNoPosition,
                                    function_type,
                                    FunctionLiteral::NORMAL_ARITY, &ok);
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
                                 bool is_eval, Scope** eval_scope, bool* ok) {
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

    Token::Value token = peek();
    Scanner::Location token_loc = scanner()->peek_location();
    Scanner::Location old_super_loc = function_state_->super_call_location();
    Statement* stat = ParseStatementListItem(CHECK_OK);
    Scanner::Location super_loc = function_state_->super_call_location();

    if (is_strong(language_mode()) &&
        i::IsConstructor(function_state_->kind()) &&
        !old_super_loc.IsValid() && super_loc.IsValid() &&
        token != Token::SUPER) {
      // TODO(rossberg): This is more permissive than spec'ed, it allows e.g.
      //   super(), 1;
      //   super() + "";
      //   super() = 0;
      // That should still be safe, though, thanks to left-to-right evaluation.
      // The proper check would be difficult to implement in the preparser.
      ReportMessageAt(super_loc, "strong_super_call_nested");
      *ok = false;
      return NULL;
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
            // TODO(mstarzinger): Global strict eval calls, need their own scope
            // as specified in ES5 10.4.2(3). The correct fix would be to always
            // add this scope in DoParseProgram(), but that requires adaptations
            // all over the code base, so we go with a quick-fix for now.
            // In the same manner, we have to patch the parsing mode.
            if (is_eval && !scope_->is_eval_scope()) {
              DCHECK(scope_->is_script_scope());
              Scope* scope = NewScope(scope_, EVAL_SCOPE);
              scope->set_start_position(scope_->start_position());
              scope->set_end_position(scope_->end_position());
              scope_ = scope;
              if (eval_scope != NULL) {
                // Caller will correct the positions of the ad hoc eval scope.
                *eval_scope = scope;
              }
              mode_ = PARSE_EAGERLY;
            }
            scope_->SetLanguageMode(static_cast<LanguageMode>(
                scope_->language_mode() | STRICT_BIT));
          }

          if (use_strong_found) {
            scope_->SetLanguageMode(static_cast<LanguageMode>(
                scope_->language_mode() | STRONG_BIT));
          }
        } else if (literal->raw_value()->AsString() ==
                       ast_value_factory()->use_asm_string() &&
                   token_loc.end_pos - token_loc.beg_pos ==
                       ast_value_factory()->use_asm_string()->length() + 2) {
          // Store the usage count; The actual use counter on the isolate is
          // incremented after parsing is done.
          ++use_counts_[v8::Isolate::kUseAsm];
          scope_->SetAsmModule();
        }
      } else {
        // End of the directive prologue.
        directive_prologue = false;
      }
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
      return ParseClassDeclaration(NULL, ok);
    case Token::CONST:
    case Token::VAR:
      return ParseVariableStatement(kStatementListItem, NULL, ok);
    case Token::LET:
      if (is_strict(language_mode())) {
        return ParseVariableStatement(kStatementListItem, NULL, ok);
      }
      // Fall through.
    default:
      return ParseStatement(NULL, ok);
  }
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
  scope_->SetLanguageMode(
      static_cast<LanguageMode>(scope_->language_mode() | STRICT_BIT));

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
      ParserTraits::ReportMessage("module_export_undefined", it.local_name());
      *ok = false;
      return NULL;
    }
  }

  scope_->module()->Freeze();
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
      ReportMessage("unexpected_reserved");
      return NULL;
    } else if (IsEvalOrArguments(local_name)) {
      *ok = false;
      ReportMessage("strict_eval_arguments");
      return NULL;
    }
    VariableProxy* proxy = NewUnresolved(local_name, IMPORT);
    ImportDeclaration* declaration =
        factory()->NewImportDeclaration(proxy, import_name, NULL, scope_, pos);
    Declare(declaration, true, CHECK_OK);
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
    ExpectSemicolon(CHECK_OK);
    // TODO(ES6): Add module to the requested modules of scope_->module().
    USE(module_specifier);
    return factory()->NewEmptyStatement(pos);
  }

  // Parse ImportedDefaultBinding if present.
  ImportDeclaration* import_default_declaration = NULL;
  if (tok != Token::MUL && tok != Token::LBRACE) {
    const AstRawString* local_name =
        ParseIdentifier(kDontAllowEvalOrArguments, CHECK_OK);
    VariableProxy* proxy = NewUnresolved(local_name, IMPORT);
    import_default_declaration = factory()->NewImportDeclaration(
        proxy, ast_value_factory()->default_string(), NULL, scope_, pos);
    Declare(import_default_declaration, true, CHECK_OK);
  }

  const AstRawString* module_instance_binding = NULL;
  ZoneList<ImportDeclaration*>* named_declarations = NULL;
  if (import_default_declaration == NULL || Check(Token::COMMA)) {
    switch (peek()) {
      case Token::MUL: {
        Consume(Token::MUL);
        ExpectContextualKeyword(CStrVector("as"), CHECK_OK);
        module_instance_binding =
            ParseIdentifier(kDontAllowEvalOrArguments, CHECK_OK);
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
  ExpectSemicolon(CHECK_OK);

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

  return factory()->NewEmptyStatement(pos);
}


Statement* Parser::ParseExportDefault(bool* ok) {
  //  Supports the following productions, starting after the 'default' token:
  //    'export' 'default' FunctionDeclaration
  //    'export' 'default' ClassDeclaration
  //    'export' 'default' AssignmentExpression[In] ';'

  Expect(Token::DEFAULT, CHECK_OK);
  Scanner::Location default_loc = scanner()->location();

  ZoneList<const AstRawString*> names(1, zone());
  Statement* result = NULL;
  switch (peek()) {
    case Token::FUNCTION:
      // TODO(ES6): Support parsing anonymous function declarations here.
      result = ParseFunctionDeclaration(&names, CHECK_OK);
      break;

    case Token::CLASS:
      // TODO(ES6): Support parsing anonymous class declarations here.
      result = ParseClassDeclaration(&names, CHECK_OK);
      break;

    default: {
      int pos = peek_position();
      Expression* expr = ParseAssignmentExpression(true, CHECK_OK);
      ExpectSemicolon(CHECK_OK);
      result = factory()->NewExpressionStatement(expr, pos);
      break;
    }
  }

  const AstRawString* default_string = ast_value_factory()->default_string();

  DCHECK_LE(names.length(), 1);
  if (names.length() == 1) {
    scope_->module()->AddLocalExport(default_string, names.first(), zone(), ok);
    if (!*ok) {
      ParserTraits::ReportMessageAt(default_loc, "duplicate_export",
                                    default_string);
      return NULL;
    }
  } else {
    // TODO(ES6): Assign result to a const binding with the name "*default*"
    // and add an export entry with "*default*" as the local name.
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
      ExpectSemicolon(CHECK_OK);
      // TODO(ES6): scope_->module()->AddStarExport(...)
      USE(module_specifier);
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
        ReportMessageAt(reserved_loc, "unexpected_reserved");
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
                                          "duplicate_export", export_names[i]);
            return NULL;
          }
        }
      } else {
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
      ParserTraits::ReportMessage("duplicate_export", names[i]);
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
        ReportMessageAt(scanner()->peek_location(), "strong_empty");
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
      return ParseContinueStatement(ok);

    case Token::BREAK:
      return ParseBreakStatement(labels, ok);

    case Token::RETURN:
      return ParseReturnStatement(ok);

    case Token::WITH:
      return ParseWithStatement(labels, ok);

    case Token::SWITCH:
      return ParseSwitchStatement(labels, ok);

    case Token::THROW:
      return ParseThrowStatement(ok);

    case Token::TRY: {
      // NOTE: It is somewhat complicated to have labels on
      // try-statements. When breaking out of a try-finally statement,
      // one must take great care not to treat it as a
      // fall-through. It is much easier just to wrap the entire
      // try-statement in a statement block and put the labels there
      Block* result =
          factory()->NewBlock(labels, 1, false, RelocInfo::kNoPosition);
      Target target(&this->target_stack_, result);
      TryStatement* statement = ParseTryStatement(CHECK_OK);
      if (result) result->AddStatement(statement, zone());
      return result;
    }

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
        ReportMessageAt(scanner()->peek_location(), "strict_function");
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
      if (is_sloppy(language_mode())) {
        return ParseVariableStatement(kStatement, NULL, ok);
      }

    // Fall through.
    default:
      return ParseExpressionOrLabelledStatement(labels, ok);
  }
}


VariableProxy* Parser::NewUnresolved(const AstRawString* name,
                                     VariableMode mode) {
  // If we are inside a function, a declaration of a var/const variable is a
  // truly local variable, and the scope of the variable is always the function
  // scope.
  // Let/const variables in harmony mode are always added to the immediately
  // enclosing scope.
  return DeclarationScope(mode)->NewUnresolved(factory(), name,
                                               scanner()->location().beg_pos,
                                               scanner()->location().end_pos);
}


Variable* Parser::Declare(Declaration* declaration, bool resolve, bool* ok) {
  VariableProxy* proxy = declaration->proxy();
  DCHECK(proxy->raw_name() != NULL);
  const AstRawString* name = proxy->raw_name();
  VariableMode mode = declaration->mode();
  Scope* declaration_scope = DeclarationScope(mode);
  Variable* var = NULL;

  // If a suitable scope exists, then we can statically declare this
  // variable and also set its mode. In any case, a Declaration node
  // will be added to the scope so that the declaration can be added
  // to the corresponding activation frame at runtime if necessary.
  // For instance declarations inside an eval scope need to be added
  // to the calling function context.
  // Similarly, strict mode eval scope does not leak variable declarations to
  // the caller's scope so we declare all locals, too.
  if (declaration_scope->is_function_scope() ||
      declaration_scope->is_strict_eval_scope() ||
      declaration_scope->is_block_scope() ||
      declaration_scope->is_module_scope() ||
      declaration_scope->is_script_scope()) {
    // Declare the variable in the declaration scope.
    var = declaration_scope->LookupLocal(name);
    if (var == NULL) {
      // Declare the name.
      var = declaration_scope->DeclareLocal(
          name, mode, declaration->initialization(),
          declaration->IsFunctionDeclaration() ? Variable::FUNCTION
                                               : Variable::NORMAL,
          kNotAssigned);
    } else if (IsLexicalVariableMode(mode) ||
               IsLexicalVariableMode(var->mode()) ||
               ((mode == CONST_LEGACY || var->mode() == CONST_LEGACY) &&
                !declaration_scope->is_script_scope())) {
      // The name was declared in this scope before; check for conflicting
      // re-declarations. We have a conflict if either of the declarations is
      // not a var (in script scope, we also have to ignore legacy const for
      // compatibility). There is similar code in runtime.cc in the Declare
      // functions. The function CheckConflictingVarDeclarations checks for
      // var and let bindings from different scopes whereas this is a check for
      // conflicting declarations within the same scope. This check also covers
      // the special case
      //
      // function () { let x; { var x; } }
      //
      // because the var declaration is hoisted to the function scope where 'x'
      // is already bound.
      DCHECK(IsDeclaredVariableMode(var->mode()));
      if (is_strict(language_mode())) {
        // In harmony we treat re-declarations as early errors. See
        // ES5 16 for a definition of early errors.
        ParserTraits::ReportMessage("var_redeclaration", name);
        *ok = false;
        return nullptr;
      }
      Expression* expression = NewThrowTypeError(
          "var_redeclaration", name, declaration->position());
      declaration_scope->SetIllegalRedeclaration(expression);
    } else if (mode == VAR) {
      var->set_maybe_assigned();
    }
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
  } else if (declaration_scope->is_eval_scope() &&
             is_sloppy(declaration_scope->language_mode())) {
    // For variable declarations in a sloppy eval scope the proxy is bound
    // to a lookup variable to force a dynamic declaration using the
    // DeclareLookupSlot runtime function.
    Variable::Kind kind = Variable::NORMAL;
    // TODO(sigurds) figure out if kNotAssigned is OK here
    var = new (zone()) Variable(declaration_scope, name, mode, kind,
                                declaration->initialization(), kNotAssigned);
    var->AllocateTo(Variable::LOOKUP, -1);
    resolve = true;
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
  const AstRawString* name = ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  bool done = (peek() == Token::RPAREN);
  while (!done) {
    ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
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
  DeclarationScope(VAR)->ForceEagerCompilation();

  // TODO(1240846): It's weird that native function declarations are
  // introduced dynamically when we meet their declarations, whereas
  // other functions are set up when entering the surrounding scope.
  VariableProxy* proxy = NewUnresolved(name, VAR);
  Declaration* declaration =
      factory()->NewVariableDeclaration(proxy, VAR, scope_, pos);
  Declare(declaration, true, CHECK_OK);
  NativeFunctionLiteral* lit = factory()->NewNativeFunctionLiteral(
      name, extension_, RelocInfo::kNoPosition);
  return factory()->NewExpressionStatement(
      factory()->NewAssignment(
          Token::INIT_VAR, proxy, lit, RelocInfo::kNoPosition),
      pos);
}


Statement* Parser::ParseFunctionDeclaration(
    ZoneList<const AstRawString*>* names, bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameterListopt ')' '{' FunctionBody '}'
  // GeneratorDeclaration ::
  //   'function' '*' Identifier '(' FormalParameterListopt ')'
  //      '{' FunctionBody '}'
  Expect(Token::FUNCTION, CHECK_OK);
  int pos = position();
  bool is_generator = Check(Token::MUL);
  bool is_strict_reserved = false;
  const AstRawString* name = ParseIdentifierOrStrictReservedWord(
      &is_strict_reserved, CHECK_OK);
  FunctionLiteral* fun =
      ParseFunctionLiteral(name, scanner()->location(), is_strict_reserved,
                           is_generator ? FunctionKind::kGeneratorFunction
                                        : FunctionKind::kNormalFunction,
                           pos, FunctionLiteral::DECLARATION,
                           FunctionLiteral::NORMAL_ARITY, CHECK_OK);
  // Even if we're not at the top-level of the global or a function
  // scope, we treat it as such and introduce the function with its
  // initial value upon entering the corresponding scope.
  // In ES6, a function behaves as a lexical binding, except in
  // a script scope, or the initial scope of eval or another function.
  VariableMode mode =
      is_strong(language_mode())
          ? CONST
          : is_strict(language_mode()) &&
                    !(scope_->is_script_scope() || scope_->is_eval_scope() ||
                      scope_->is_function_scope())
                ? LET
                : VAR;
  VariableProxy* proxy = NewUnresolved(name, mode);
  Declaration* declaration =
      factory()->NewFunctionDeclaration(proxy, mode, fun, scope_, pos);
  Declare(declaration, true, CHECK_OK);
  if (names) names->Add(name, zone());
  return factory()->NewEmptyStatement(RelocInfo::kNoPosition);
}


Statement* Parser::ParseClassDeclaration(ZoneList<const AstRawString*>* names,
                                         bool* ok) {
  // ClassDeclaration ::
  //   'class' Identifier ('extends' LeftHandExpression)? '{' ClassBody '}'
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

  Expect(Token::CLASS, CHECK_OK);
  if (!allow_harmony_sloppy() && is_sloppy(language_mode())) {
    ReportMessage("sloppy_lexical");
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
  Declare(declaration, true, CHECK_OK);
  proxy->var()->set_initializer_position(position());

  Token::Value init_op =
      is_strong(language_mode()) ? Token::INIT_CONST : Token::INIT_LET;
  Assignment* assignment = factory()->NewAssignment(init_op, proxy, value, pos);
  Statement* assignment_statement =
      factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
  if (names) names->Add(name, zone());
  return assignment_statement;
}


Block* Parser::ParseBlock(ZoneList<const AstRawString*>* labels, bool* ok) {
  if (is_strict(language_mode())) {
    return ParseScopedBlock(labels, ok);
  }

  // Block ::
  //   '{' Statement* '}'

  // Note that a Block does not introduce a new execution scope!
  // (ECMA-262, 3rd, 12.2)
  //
  // Construct block expecting 16 statements.
  Block* result =
      factory()->NewBlock(labels, 16, false, RelocInfo::kNoPosition);
  Target target(&this->target_stack_, result);
  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    Statement* stat = ParseStatement(NULL, CHECK_OK);
    if (stat && !stat->IsEmpty()) {
      result->AddStatement(stat, zone());
    }
  }
  Expect(Token::RBRACE, CHECK_OK);
  return result;
}


Block* Parser::ParseScopedBlock(ZoneList<const AstRawString*>* labels,
                                bool* ok) {
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
        body->AddStatement(stat, zone());
      }
    }
  }
  Expect(Token::RBRACE, CHECK_OK);
  block_scope->set_end_position(scanner()->location().end_pos);
  block_scope = block_scope->FinalizeBlockScope();
  body->set_scope(block_scope);
  return body;
}


Block* Parser::ParseVariableStatement(VariableDeclarationContext var_context,
                                      ZoneList<const AstRawString*>* names,
                                      bool* ok) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  const AstRawString* ignore;
  Block* result =
      ParseVariableDeclarations(var_context, NULL, names, &ignore, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return result;
}


// If the variable declaration declares exactly one non-const
// variable, then *out is set to that variable. In all other cases,
// *out is untouched; in particular, it is the caller's responsibility
// to initialize it properly. This mechanism is used for the parsing
// of 'for-in' loops.
Block* Parser::ParseVariableDeclarations(
    VariableDeclarationContext var_context,
    VariableDeclarationProperties* decl_props,
    ZoneList<const AstRawString*>* names,
    const AstRawString** out,
    bool* ok) {
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

  int pos = peek_position();
  VariableMode mode = VAR;
  // True if the binding needs initialization. 'let' and 'const' declared
  // bindings are created uninitialized by their declaration nodes and
  // need initialization. 'var' declared bindings are always initialized
  // immediately by their declaration nodes.
  bool needs_init = false;
  bool is_const = false;
  Token::Value init_op = Token::INIT_VAR;
  if (peek() == Token::VAR) {
    if (is_strong(language_mode())) {
      Scanner::Location location = scanner()->peek_location();
      ReportMessageAt(location, "strong_var");
      *ok = false;
      return NULL;
    }
    Consume(Token::VAR);
  } else if (peek() == Token::CONST) {
    Consume(Token::CONST);
    if (is_sloppy(language_mode())) {
      mode = CONST_LEGACY;
      init_op = Token::INIT_CONST_LEGACY;
    } else {
      DCHECK(var_context != kStatement);
      mode = CONST;
      init_op = Token::INIT_CONST;
    }
    is_const = true;
    needs_init = true;
  } else if (peek() == Token::LET && is_strict(language_mode())) {
    Consume(Token::LET);
    DCHECK(var_context != kStatement);
    mode = LET;
    needs_init = true;
    init_op = Token::INIT_LET;
  } else {
    UNREACHABLE();  // by current callers
  }

  Scope* declaration_scope = DeclarationScope(mode);

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
  //
  // Create new block with one expected declaration.
  Block* block = factory()->NewBlock(NULL, 1, true, pos);
  int nvars = 0;  // the number of variables declared
  const AstRawString* name = NULL;
  bool is_for_iteration_variable;
  do {
    if (fni_ != NULL) fni_->Enter();

    // Parse variable name.
    if (nvars > 0) Consume(Token::COMMA);
    name = ParseIdentifier(kDontAllowEvalOrArguments, CHECK_OK);
    if (fni_ != NULL) fni_->PushVariableName(name);

    // Declare variable.
    // Note that we *always* must treat the initial value via a separate init
    // assignment for variables and constants because the value must be assigned
    // when the variable is encountered in the source. But the variable/constant
    // is declared (and set to 'undefined') upon entering the function within
    // which the variable or constant is declared. Only function variables have
    // an initial value in the declaration (because they are initialized upon
    // entering the function).
    //
    // If we have a const declaration, in an inner scope, the proxy is always
    // bound to the declared variable (independent of possibly surrounding with
    // statements).
    // For let/const declarations in harmony mode, we can also immediately
    // pre-resolve the proxy because it resides in the same scope as the
    // declaration.
    is_for_iteration_variable =
        var_context == kForStatement &&
        (peek() == Token::IN || PeekContextualKeyword(CStrVector("of")));
    if (is_for_iteration_variable && mode == CONST) {
      needs_init = false;
    }

    VariableProxy* proxy = NewUnresolved(name, mode);
    Declaration* declaration =
        factory()->NewVariableDeclaration(proxy, mode, scope_, pos);
    Variable* var = Declare(declaration, mode != VAR, CHECK_OK);
    DCHECK_NOT_NULL(var);
    DCHECK(!proxy->is_resolved() || proxy->var() == var);
    nvars++;
    if (declaration_scope->num_var_or_const() > kMaxNumFunctionLocals) {
      ReportMessage("too_many_variables");
      *ok = false;
      return NULL;
    }
    if (names) names->Add(name, zone());

    // Parse initialization expression if present and/or needed. A
    // declaration of the form:
    //
    //    var v = x;
    //
    // is syntactic sugar for:
    //
    //    var v; v = x;
    //
    // In particular, we need to re-lookup 'v' (in scope_, not
    // declaration_scope) as it may be a different 'v' than the 'v' in the
    // declaration (e.g., if we are inside a 'with' statement or 'catch'
    // block).
    //
    // However, note that const declarations are different! A const
    // declaration of the form:
    //
    //   const c = x;
    //
    // is *not* syntactic sugar for:
    //
    //   const c; c = x;
    //
    // The "variable" c initialized to x is the same as the declared
    // one - there is no re-lookup (see the last parameter of the
    // Declare() call above).

    Scope* initialization_scope = is_const ? declaration_scope : scope_;
    Expression* value = NULL;
    int pos = -1;
    // Harmony consts have non-optional initializers.
    if (peek() == Token::ASSIGN ||
        (mode == CONST && !is_for_iteration_variable)) {
      Expect(Token::ASSIGN, CHECK_OK);
      pos = position();
      value = ParseAssignmentExpression(var_context != kForStatement, CHECK_OK);
      // Don't infer if it is "a = function(){...}();"-like expression.
      if (fni_ != NULL &&
          value->AsCall() == NULL &&
          value->AsCallNew() == NULL) {
        fni_->Infer();
      } else {
        fni_->RemoveLastFunction();
      }
      if (decl_props != NULL) *decl_props = kHasInitializers;
      // End position of the initializer is after the assignment expression.
      var->set_initializer_position(scanner()->location().end_pos);
    } else {
      // End position of the initializer is after the variable.
      var->set_initializer_position(position());
    }

    // Make sure that 'const x' and 'let x' initialize 'x' to undefined.
    if (value == NULL && needs_init) {
      value = GetLiteralUndefined(position());
    }

    // Global variable declarations must be compiled in a specific
    // way. When the script containing the global variable declaration
    // is entered, the global variable must be declared, so that if it
    // doesn't exist (on the global object itself, see ES5 errata) it
    // gets created with an initial undefined value. This is handled
    // by the declarations part of the function representing the
    // top-level global code; see Runtime::DeclareGlobalVariable. If
    // it already exists (in the object or in a prototype), it is
    // *not* touched until the variable declaration statement is
    // executed.
    //
    // Executing the variable declaration statement will always
    // guarantee to give the global object an own property.
    // This way, global variable declarations can shadow
    // properties in the prototype chain, but only after the variable
    // declaration statement has been executed. This is important in
    // browsers where the global object (window) has lots of
    // properties defined in prototype objects.
    if (initialization_scope->is_script_scope() &&
        !IsLexicalVariableMode(mode)) {
      // Compute the arguments for the runtime call.
      ZoneList<Expression*>* arguments =
          new(zone()) ZoneList<Expression*>(3, zone());
      // We have at least 1 parameter.
      arguments->Add(factory()->NewStringLiteral(name, pos), zone());
      CallRuntime* initialize;

      if (is_const) {
        arguments->Add(value, zone());
        value = NULL;  // zap the value to avoid the unnecessary assignment

        // Construct the call to Runtime_InitializeConstGlobal
        // and add it to the initialization statement block.
        // Note that the function does different things depending on
        // the number of arguments (1 or 2).
        initialize = factory()->NewCallRuntime(
            ast_value_factory()->initialize_const_global_string(),
            Runtime::FunctionForId(Runtime::kInitializeConstGlobal), arguments,
            pos);
      } else {
        // Add language mode.
        // We may want to pass singleton to avoid Literal allocations.
        LanguageMode language_mode = initialization_scope->language_mode();
        arguments->Add(factory()->NewNumberLiteral(language_mode, pos), zone());

        // Be careful not to assign a value to the global variable if
        // we're in a with. The initialization value should not
        // necessarily be stored in the global object in that case,
        // which is why we need to generate a separate assignment node.
        if (value != NULL && !inside_with()) {
          arguments->Add(value, zone());
          value = NULL;  // zap the value to avoid the unnecessary assignment
          // Construct the call to Runtime_InitializeVarGlobal
          // and add it to the initialization statement block.
          initialize = factory()->NewCallRuntime(
              ast_value_factory()->initialize_var_global_string(),
              Runtime::FunctionForId(Runtime::kInitializeVarGlobal), arguments,
              pos);
        } else {
          initialize = NULL;
        }
      }

      if (initialize != NULL) {
        block->AddStatement(factory()->NewExpressionStatement(
                                initialize, RelocInfo::kNoPosition),
                            zone());
      }
    } else if (needs_init) {
      // Constant initializations always assign to the declared constant which
      // is always at the function scope level. This is only relevant for
      // dynamically looked-up variables and constants (the start context for
      // constant lookups is always the function context, while it is the top
      // context for var declared variables). Sigh...
      // For 'let' and 'const' declared variables in harmony mode the
      // initialization also always assigns to the declared variable.
      DCHECK(proxy != NULL);
      DCHECK(proxy->var() != NULL);
      DCHECK(value != NULL);
      Assignment* assignment =
          factory()->NewAssignment(init_op, proxy, value, pos);
      block->AddStatement(
          factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition),
          zone());
      value = NULL;
    }

    // Add an assignment node to the initialization statement block if we still
    // have a pending initialization value.
    if (value != NULL) {
      DCHECK(mode == VAR);
      // 'var' initializations are simply assignments (with all the consequences
      // if they are inside a 'with' statement - they may change a 'with' object
      // property).
      VariableProxy* proxy =
          initialization_scope->NewUnresolved(factory(), name);
      Assignment* assignment =
          factory()->NewAssignment(init_op, proxy, value, pos);
      block->AddStatement(
          factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition),
          zone());
    }

    if (fni_ != NULL) fni_->Leave();
  } while (peek() == Token::COMMA);

  // If there was a single non-const declaration, return it in the output
  // parameter for possible use by for/in.
  if (nvars == 1 && (!is_const || is_for_iteration_variable)) {
    *out = name;
  }

  return block;
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

  switch (peek()) {
    case Token::FUNCTION:
    case Token::LBRACE:
      UNREACHABLE();  // Always handled by the callers.
    case Token::CLASS:
      ReportUnexpectedToken(Next());
      *ok = false;
      return nullptr;

    // TODO(arv): Handle `let [`
    // https://code.google.com/p/v8/issues/detail?id=3847

    default:
      break;
  }

  int pos = peek_position();
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
      ParserTraits::ReportMessage("label_redeclaration", label);
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
  if (peek() == Token::IDENTIFIER && expr->AsVariableProxy() != NULL &&
      expr->AsVariableProxy()->raw_name() ==
          ast_value_factory()->let_string()) {
    ReportMessage("sloppy_lexical", NULL);
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
    label = ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
  }
  IterationStatement* target = LookupContinueTarget(label, CHECK_OK);
  if (target == NULL) {
    // Illegal continue statement.
    const char* message = "illegal_continue";
    if (label != NULL) {
      message = "unknown_label";
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
    label = ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
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
    const char* message = "illegal_break";
    if (label != NULL) {
      message = "unknown_label";
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
        i::IsConstructor(function_state_->kind())) {
      int pos = peek_position();
      ReportMessageAt(Scanner::Location(pos, pos + 1),
                      "strong_constructor_return_value");
      *ok = false;
      return NULL;
    }
    return_value = ParseExpression(true, CHECK_OK);
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
    ReportMessageAt(loc, "illegal_return");
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
    ReportMessage("strict_mode_with");
    *ok = false;
    return NULL;
  }

  Expect(Token::LPAREN, CHECK_OK);
  Expression* expr = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  scope_->DeclarationScope()->RecordWithStatement();
  Scope* with_scope = NewScope(scope_, WITH_SCOPE);
  Statement* stmt;
  { BlockState block_state(&scope_, with_scope);
    with_scope->set_start_position(scanner()->peek_location().beg_pos);
    stmt = ParseSubStatement(labels, CHECK_OK);
    with_scope->set_end_position(scanner()->location().end_pos);
  }
  return factory()->NewWithStatement(with_scope, expr, stmt, pos);
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
      ReportMessage("multiple_defaults_in_switch");
      *ok = false;
      return NULL;
    }
    *default_seen_ptr = true;
  }
  Expect(Token::COLON, CHECK_OK);
  int pos = position();
  ZoneList<Statement*>* statements =
      new(zone()) ZoneList<Statement*>(5, zone());
  while (peek() != Token::CASE &&
         peek() != Token::DEFAULT &&
         peek() != Token::RBRACE) {
    Statement* stat = ParseStatementListItem(CHECK_OK);
    statements->Add(stat, zone());
  }

  return factory()->NewCaseClause(label, statements, pos);
}


SwitchStatement* Parser::ParseSwitchStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'

  SwitchStatement* statement =
      factory()->NewSwitchStatement(labels, peek_position());
  Target target(&this->target_stack_, statement);

  Expect(Token::SWITCH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  Expression* tag = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  bool default_seen = false;
  ZoneList<CaseClause*>* cases = new(zone()) ZoneList<CaseClause*>(4, zone());
  Expect(Token::LBRACE, CHECK_OK);
  while (peek() != Token::RBRACE) {
    CaseClause* clause = ParseCaseClause(&default_seen, CHECK_OK);
    cases->Add(clause, zone());
  }
  Expect(Token::RBRACE, CHECK_OK);

  if (statement) statement->Initialize(tag, cases);
  return statement;
}


Statement* Parser::ParseThrowStatement(bool* ok) {
  // ThrowStatement ::
  //   'throw' Expression ';'

  Expect(Token::THROW, CHECK_OK);
  int pos = position();
  if (scanner()->HasAnyLineTerminatorBeforeNext()) {
    ReportMessage("newline_after_throw");
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

  Block* try_block = ParseBlock(NULL, CHECK_OK);

  Token::Value tok = peek();
  if (tok != Token::CATCH && tok != Token::FINALLY) {
    ReportMessage("no_catch_or_finally");
    *ok = false;
    return NULL;
  }

  Scope* catch_scope = NULL;
  Variable* catch_variable = NULL;
  Block* catch_block = NULL;
  const AstRawString* name = NULL;
  if (tok == Token::CATCH) {
    Consume(Token::CATCH);

    Expect(Token::LPAREN, CHECK_OK);
    catch_scope = NewScope(scope_, CATCH_SCOPE);
    catch_scope->set_start_position(scanner()->location().beg_pos);
    name = ParseIdentifier(kDontAllowEvalOrArguments, CHECK_OK);

    Expect(Token::RPAREN, CHECK_OK);

    catch_variable = catch_scope->DeclareLocal(name, VAR, kCreatedInitialized,
                                               Variable::NORMAL);
    BlockState block_state(&scope_, catch_scope);
    catch_block = ParseBlock(NULL, CHECK_OK);

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
    int index = function_state_->NextHandlerIndex();
    TryCatchStatement* statement = factory()->NewTryCatchStatement(
        index, try_block, catch_scope, catch_variable, catch_block,
        RelocInfo::kNoPosition);
    try_block = factory()->NewBlock(NULL, 1, false, RelocInfo::kNoPosition);
    try_block->AddStatement(statement, zone());
    catch_block = NULL;  // Clear to indicate it's been handled.
  }

  TryStatement* result = NULL;
  if (catch_block != NULL) {
    DCHECK(finally_block == NULL);
    DCHECK(catch_scope != NULL && catch_variable != NULL);
    int index = function_state_->NextHandlerIndex();
    result = factory()->NewTryCatchStatement(
        index, try_block, catch_scope, catch_variable, catch_block, pos);
  } else {
    DCHECK(finally_block != NULL);
    int index = function_state_->NextHandlerIndex();
    result = factory()->NewTryFinallyStatement(
        index, try_block, finally_block, pos);
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


void Parser::InitializeForEachStatement(ForEachStatement* stmt,
                                        Expression* each,
                                        Expression* subject,
                                        Statement* body) {
  ForOfStatement* for_of = stmt->AsForOfStatement();

  if (for_of != NULL) {
    Variable* iterator = scope_->DeclarationScope()->NewTemporary(
        ast_value_factory()->dot_iterator_string());
    Variable* result = scope_->DeclarationScope()->NewTemporary(
        ast_value_factory()->dot_result_string());

    Expression* assign_iterator;
    Expression* next_result;
    Expression* result_done;
    Expression* assign_each;

    // iterator = subject[Symbol.iterator]()
    assign_iterator = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(iterator),
        GetIterator(subject, factory()), subject->position());

    // !%_IsSpecObject(result = iterator.next()) &&
    //     %ThrowIteratorResultNotAnObject(result)
    {
      // result = iterator.next()
      Expression* iterator_proxy = factory()->NewVariableProxy(iterator);
      Expression* next_literal = factory()->NewStringLiteral(
          ast_value_factory()->next_string(), RelocInfo::kNoPosition);
      Expression* next_property = factory()->NewProperty(
          iterator_proxy, next_literal, RelocInfo::kNoPosition);
      ZoneList<Expression*>* next_arguments =
          new (zone()) ZoneList<Expression*>(0, zone());
      Expression* next_call = factory()->NewCall(next_property, next_arguments,
                                                 subject->position());
      Expression* result_proxy = factory()->NewVariableProxy(result);
      next_result = factory()->NewAssignment(Token::ASSIGN, result_proxy,
                                             next_call, subject->position());

      // %_IsSpecObject(...)
      ZoneList<Expression*>* is_spec_object_args =
          new (zone()) ZoneList<Expression*>(1, zone());
      is_spec_object_args->Add(next_result, zone());
      Expression* is_spec_object_call = factory()->NewCallRuntime(
          ast_value_factory()->is_spec_object_string(),
          Runtime::FunctionForId(Runtime::kInlineIsSpecObject),
          is_spec_object_args, subject->position());

      // %ThrowIteratorResultNotAnObject(result)
      Expression* result_proxy_again = factory()->NewVariableProxy(result);
      ZoneList<Expression*>* throw_arguments =
          new (zone()) ZoneList<Expression*>(1, zone());
      throw_arguments->Add(result_proxy_again, zone());
      Expression* throw_call = factory()->NewCallRuntime(
          ast_value_factory()->throw_iterator_result_not_an_object_string(),
          Runtime::FunctionForId(Runtime::kThrowIteratorResultNotAnObject),
          throw_arguments, subject->position());

      next_result = factory()->NewBinaryOperation(
          Token::AND, factory()->NewUnaryOperation(
                          Token::NOT, is_spec_object_call, subject->position()),
          throw_call, subject->position());
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
                                             each->position());
    }

    for_of->Initialize(each, subject, body,
                       assign_iterator,
                       next_result,
                       result_done,
                       assign_each);
  } else {
    stmt->Initialize(each, subject, body);
  }
}


Statement* Parser::DesugarLexicalBindingsInForStatement(
    Scope* inner_scope, bool is_const, ZoneList<const AstRawString*>* names,
    ForStatement* loop, Statement* init, Expression* cond, Statement* next,
    Statement* body, bool* ok) {
  // ES6 13.6.3.4 specifies that on each loop iteration the let variables are
  // copied into a new environment. After copying, the "next" statement of the
  // loop is executed to update the loop variables. The loop condition is
  // checked and the loop body is executed.
  //
  // We rewrite a for statement of the form
  //
  //  labels: for (let/const x = i; cond; next) body
  //
  // into
  //
  //  {
  //    let/const x = i;
  //    temp_x = x;
  //    first = 1;
  //    outer: for (;;) {
  //      let/const x = temp_x;
  //      if (first == 1) {
  //        first = 0;
  //      } else {
  //        next;
  //      }
  //      flag = 1;
  //      labels: for (; flag == 1; flag = 0, temp_x = x) {
  //        if (cond) {
  //          body
  //        } else {
  //          break outer;
  //        }
  //      }
  //      if (flag == 1) {
  //        break;
  //      }
  //    }
  //  }

  DCHECK(names->length() > 0);
  Scope* for_scope = scope_;
  ZoneList<Variable*> temps(names->length(), zone());

  Block* outer_block = factory()->NewBlock(NULL, names->length() + 3, false,
                                           RelocInfo::kNoPosition);

  // Add statement: let/const x = i.
  outer_block->AddStatement(init, zone());

  const AstRawString* temp_name = ast_value_factory()->dot_for_string();

  // For each lexical variable x:
  //   make statement: temp_x = x.
  for (int i = 0; i < names->length(); i++) {
    VariableProxy* proxy = NewUnresolved(names->at(i), LET);
    Variable* temp = scope_->DeclarationScope()->NewTemporary(temp_name);
    VariableProxy* temp_proxy = factory()->NewVariableProxy(temp);
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, temp_proxy, proxy, RelocInfo::kNoPosition);
    Statement* assignment_statement = factory()->NewExpressionStatement(
        assignment, RelocInfo::kNoPosition);
    outer_block->AddStatement(assignment_statement, zone());
    temps.Add(temp, zone());
  }

  Variable* first = NULL;
  // Make statement: first = 1.
  if (next) {
    first = scope_->DeclarationScope()->NewTemporary(temp_name);
    VariableProxy* first_proxy = factory()->NewVariableProxy(first);
    Expression* const1 = factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, first_proxy, const1, RelocInfo::kNoPosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
    outer_block->AddStatement(assignment_statement, zone());
  }

  // Make statement: outer: for (;;)
  // Note that we don't actually create the label, or set this loop up as an
  // explicit break target, instead handing it directly to those nodes that
  // need to know about it. This should be safe because we don't run any code
  // in this function that looks up break targets.
  ForStatement* outer_loop =
      factory()->NewForStatement(NULL, RelocInfo::kNoPosition);
  outer_block->AddStatement(outer_loop, zone());

  outer_block->set_scope(for_scope);
  scope_ = inner_scope;

  Block* inner_block = factory()->NewBlock(NULL, names->length() + 4, false,
                                           RelocInfo::kNoPosition);
  ZoneList<Variable*> inner_vars(names->length(), zone());

  // For each let variable x:
  //    make statement: let/const x = temp_x.
  VariableMode mode = is_const ? CONST : LET;
  for (int i = 0; i < names->length(); i++) {
    VariableProxy* proxy = NewUnresolved(names->at(i), mode);
    Declaration* declaration = factory()->NewVariableDeclaration(
        proxy, mode, scope_, RelocInfo::kNoPosition);
    Declare(declaration, true, CHECK_OK);
    inner_vars.Add(declaration->proxy()->var(), zone());
    VariableProxy* temp_proxy = factory()->NewVariableProxy(temps.at(i));
    Assignment* assignment =
        factory()->NewAssignment(is_const ? Token::INIT_CONST : Token::INIT_LET,
                                 proxy, temp_proxy, RelocInfo::kNoPosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
    proxy->var()->set_initializer_position(init->position());
    inner_block->AddStatement(assignment_statement, zone());
  }

  // Make statement: if (first == 1) { first = 0; } else { next; }
  if (next) {
    DCHECK(first);
    Expression* compare = NULL;
    // Make compare expression: first == 1.
    {
      Expression* const1 = factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
      VariableProxy* first_proxy = factory()->NewVariableProxy(first);
      compare = factory()->NewCompareOperation(Token::EQ, first_proxy, const1,
                                               RelocInfo::kNoPosition);
    }
    Statement* clear_first = NULL;
    // Make statement: first = 0.
    {
      VariableProxy* first_proxy = factory()->NewVariableProxy(first);
      Expression* const0 = factory()->NewSmiLiteral(0, RelocInfo::kNoPosition);
      Assignment* assignment = factory()->NewAssignment(
          Token::ASSIGN, first_proxy, const0, RelocInfo::kNoPosition);
      clear_first =
          factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
    }
    Statement* clear_first_or_next =
        factory()->NewIfStatement(compare, clear_first, next, next->position());
    inner_block->AddStatement(clear_first_or_next, zone());
  }

  Variable* flag = scope_->DeclarationScope()->NewTemporary(temp_name);
  // Make statement: flag = 1.
  {
    VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
    Expression* const1 = factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, flag_proxy, const1, RelocInfo::kNoPosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition);
    inner_block->AddStatement(assignment_statement, zone());
  }

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
      Expression* const0 = factory()->NewSmiLiteral(0, RelocInfo::kNoPosition);
      compound_next = factory()->NewAssignment(Token::ASSIGN, flag_proxy,
                                               const0, RelocInfo::kNoPosition);
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

  // Make statement: if (cond) { body; } else { break outer; }
  Statement* body_or_stop = body;
  if (cond) {
    Statement* stop =
        factory()->NewBreakStatement(outer_loop, RelocInfo::kNoPosition);
    body_or_stop =
        factory()->NewIfStatement(cond, body, stop, cond->position());
  }

  // Make statement: labels: for (; flag == 1; flag = 0, temp_x = x)
  // Note that we re-use the original loop node, which retains it labels
  // and ensures that any break or continue statements in body point to
  // the right place.
  loop->Initialize(NULL, flag_cond, compound_next_statement, body_or_stop);
  inner_block->AddStatement(loop, zone());

  // Make statement: if (flag == 1) { break; }
  {
    Expression* compare = NULL;
    // Make compare expresion: flag == 1.
    {
      Expression* const1 = factory()->NewSmiLiteral(1, RelocInfo::kNoPosition);
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      compare = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                               RelocInfo::kNoPosition);
    }
    Statement* stop =
        factory()->NewBreakStatement(outer_loop, RelocInfo::kNoPosition);
    Statement* empty = factory()->NewEmptyStatement(RelocInfo::kNoPosition);
    Statement* if_flag_break =
        factory()->NewIfStatement(compare, stop, empty, RelocInfo::kNoPosition);
    inner_block->AddStatement(if_flag_break, zone());
  }

  inner_scope->set_end_position(scanner()->location().end_pos);
  inner_block->set_scope(inner_scope);
  scope_ = for_scope;

  outer_loop->Initialize(NULL, NULL, NULL, inner_block);
  return outer_block;
}


Statement* Parser::ParseForStatement(ZoneList<const AstRawString*>* labels,
                                     bool* ok) {
  // ForStatement ::
  //   'for' '(' Expression? ';' Expression? ';' Expression? ')' Statement

  int stmt_pos = peek_position();
  bool is_const = false;
  Statement* init = NULL;
  ZoneList<const AstRawString*> lexical_bindings(1, zone());

  // Create an in-between scope for let-bound iteration variables.
  Scope* saved_scope = scope_;
  Scope* for_scope = NewScope(scope_, BLOCK_SCOPE);
  scope_ = for_scope;

  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  for_scope->set_start_position(scanner()->location().beg_pos);
  bool is_let_identifier_expression = false;
  if (peek() != Token::SEMICOLON) {
    if (peek() == Token::VAR ||
        (peek() == Token::CONST && is_sloppy(language_mode()))) {
      const AstRawString* name = NULL;
      VariableDeclarationProperties decl_props = kHasNoInitializers;
      Block* variable_statement =
          ParseVariableDeclarations(kForStatement, &decl_props, NULL, &name,
                                    CHECK_OK);
      bool accept_OF = decl_props == kHasNoInitializers;
      ForEachStatement::VisitMode mode;
      int each_beg_pos = scanner()->location().beg_pos;
      int each_end_pos = scanner()->location().end_pos;

      if (name != NULL && CheckInOrOf(accept_OF, &mode, ok)) {
        if (!*ok) return nullptr;
        ForEachStatement* loop =
            factory()->NewForEachStatement(mode, labels, stmt_pos);
        Target target(&this->target_stack_, loop);

        Expression* enumerable = ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);

        VariableProxy* each =
            scope_->NewUnresolved(factory(), name, each_beg_pos, each_end_pos);
        Statement* body = ParseSubStatement(NULL, CHECK_OK);
        InitializeForEachStatement(loop, each, enumerable, body);
        Block* result =
            factory()->NewBlock(NULL, 2, false, RelocInfo::kNoPosition);
        result->AddStatement(variable_statement, zone());
        result->AddStatement(loop, zone());
        scope_ = saved_scope;
        for_scope->set_end_position(scanner()->location().end_pos);
        for_scope = for_scope->FinalizeBlockScope();
        DCHECK(for_scope == NULL);
        // Parsed for-in loop w/ variable/const declaration.
        return result;
      } else {
        init = variable_statement;
      }
    } else if ((peek() == Token::LET || peek() == Token::CONST) &&
               is_strict(language_mode())) {
      is_const = peek() == Token::CONST;
      const AstRawString* name = NULL;
      VariableDeclarationProperties decl_props = kHasNoInitializers;
      Block* variable_statement =
          ParseVariableDeclarations(kForStatement, &decl_props,
                                    &lexical_bindings, &name, CHECK_OK);
      bool accept_IN = name != NULL && decl_props != kHasInitializers;
      bool accept_OF = decl_props == kHasNoInitializers;
      ForEachStatement::VisitMode mode;
      int each_beg_pos = scanner()->location().beg_pos;
      int each_end_pos = scanner()->location().end_pos;

      if (accept_IN && CheckInOrOf(accept_OF, &mode, ok)) {
        if (!*ok) return nullptr;

        // Rewrite a for-in statement of the form
        //
        //   for (let/const x in e) b
        //
        // into
        //
        //   <let x' be a temporary variable>
        //   for (x' in e) {
        //     let/const x;
        //     x = x';
        //     b;
        //   }

        // TODO(keuchel): Move the temporary variable to the block scope, after
        // implementing stack allocated block scoped variables.
        Variable* temp = scope_->DeclarationScope()->NewTemporary(
            ast_value_factory()->dot_for_string());
        VariableProxy* temp_proxy =
            factory()->NewVariableProxy(temp, each_beg_pos, each_end_pos);
        ForEachStatement* loop =
            factory()->NewForEachStatement(mode, labels, stmt_pos);
        Target target(&this->target_stack_, loop);

        // The expression does not see the loop variable.
        scope_ = saved_scope;
        Expression* enumerable = ParseExpression(true, CHECK_OK);
        scope_ = for_scope;
        Expect(Token::RPAREN, CHECK_OK);

        VariableProxy* each =
            scope_->NewUnresolved(factory(), name, each_beg_pos, each_end_pos);
        Statement* body = ParseSubStatement(NULL, CHECK_OK);
        Block* body_block =
            factory()->NewBlock(NULL, 3, false, RelocInfo::kNoPosition);
        Token::Value init_op = is_const ? Token::INIT_CONST : Token::ASSIGN;
        Assignment* assignment = factory()->NewAssignment(
            init_op, each, temp_proxy, RelocInfo::kNoPosition);
        Statement* assignment_statement = factory()->NewExpressionStatement(
            assignment, RelocInfo::kNoPosition);
        body_block->AddStatement(variable_statement, zone());
        body_block->AddStatement(assignment_statement, zone());
        body_block->AddStatement(body, zone());
        InitializeForEachStatement(loop, temp_proxy, enumerable, body_block);
        scope_ = saved_scope;
        for_scope->set_end_position(scanner()->location().end_pos);
        for_scope = for_scope->FinalizeBlockScope();
        body_block->set_scope(for_scope);
        // Parsed for-in loop w/ let declaration.
        return loop;

      } else {
        init = variable_statement;
      }
    } else {
      Scanner::Location lhs_location = scanner()->peek_location();
      Expression* expression = ParseExpression(false, CHECK_OK);
      ForEachStatement::VisitMode mode;
      bool accept_OF = expression->IsVariableProxy();
      is_let_identifier_expression =
        expression->IsVariableProxy() &&
        expression->AsVariableProxy()->raw_name() ==
            ast_value_factory()->let_string();

      if (CheckInOrOf(accept_OF, &mode, ok)) {
        if (!*ok) return nullptr;
        expression = this->CheckAndRewriteReferenceExpression(
            expression, lhs_location, "invalid_lhs_in_for", CHECK_OK);

        ForEachStatement* loop =
            factory()->NewForEachStatement(mode, labels, stmt_pos);
        Target target(&this->target_stack_, loop);

        Expression* enumerable = ParseExpression(true, CHECK_OK);
        Expect(Token::RPAREN, CHECK_OK);

        Statement* body = ParseSubStatement(NULL, CHECK_OK);
        InitializeForEachStatement(loop, expression, enumerable, body);
        scope_ = saved_scope;
        for_scope->set_end_position(scanner()->location().end_pos);
        for_scope = for_scope->FinalizeBlockScope();
        DCHECK(for_scope == NULL);
        // Parsed for-in loop.
        return loop;

      } else {
        init = factory()->NewExpressionStatement(expression, position());
      }
    }
  }

  // Standard 'for' loop
  ForStatement* loop = factory()->NewForStatement(labels, stmt_pos);
  Target target(&this->target_stack_, loop);

  // Parsed initializer at this point.
  // Detect attempts at 'let' declarations in sloppy mode.
  if (peek() == Token::IDENTIFIER && is_sloppy(language_mode()) &&
      is_let_identifier_expression) {
    ReportMessage("sloppy_lexical", NULL);
    *ok = false;
    return NULL;
  }
  Expect(Token::SEMICOLON, CHECK_OK);

  // If there are let bindings, then condition and the next statement of the
  // for loop must be parsed in a new scope.
  Scope* inner_scope = NULL;
  if (lexical_bindings.length() > 0) {
    inner_scope = NewScope(for_scope, BLOCK_SCOPE);
    inner_scope->set_start_position(scanner()->location().beg_pos);
    scope_ = inner_scope;
  }

  Expression* cond = NULL;
  if (peek() != Token::SEMICOLON) {
    cond = ParseExpression(true, CHECK_OK);
  }
  Expect(Token::SEMICOLON, CHECK_OK);

  Statement* next = NULL;
  if (peek() != Token::RPAREN) {
    int next_pos = position();
    Expression* exp = ParseExpression(true, CHECK_OK);
    next = factory()->NewExpressionStatement(exp, next_pos);
  }
  Expect(Token::RPAREN, CHECK_OK);

  Statement* body = ParseSubStatement(NULL, CHECK_OK);

  Statement* result = NULL;
  if (lexical_bindings.length() > 0) {
    scope_ = for_scope;
    result = DesugarLexicalBindingsInForStatement(
                 inner_scope, is_const, &lexical_bindings, loop, init, cond,
                 next, body, CHECK_OK);
    scope_ = saved_scope;
    for_scope->set_end_position(scanner()->location().end_pos);
  } else {
    scope_ = saved_scope;
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
      DCHECK(init != NULL);
      Block* block =
          factory()->NewBlock(NULL, 2, false, RelocInfo::kNoPosition);
      block->AddStatement(init, zone());
      block->AddStatement(loop, zone());
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


bool CheckAndDeclareArrowParameter(ParserTraits* traits, Expression* expression,
                                   Scope* scope, int* num_params,
                                   Scanner::Location* dupe_loc) {
  // Case for empty parameter lists:
  //   () => ...
  if (expression == NULL) return true;

  // Too many parentheses around expression:
  //   (( ... )) => ...
  if (expression->is_multi_parenthesized()) return false;

  // Case for a single parameter:
  //   (foo) => ...
  //   foo => ...
  if (expression->IsVariableProxy()) {
    if (expression->AsVariableProxy()->is_this()) return false;

    const AstRawString* raw_name = expression->AsVariableProxy()->raw_name();
    if (traits->IsEvalOrArguments(raw_name) ||
        traits->IsFutureStrictReserved(raw_name))
      return false;

    if (scope->IsDeclared(raw_name)) {
      *dupe_loc = Scanner::Location(
          expression->position(), expression->position() + raw_name->length());
      return false;
    }

    // When the variable was seen, it was recorded as unresolved in the outer
    // scope. But it's really not unresolved.
    scope->outer_scope()->RemoveUnresolved(expression->AsVariableProxy());

    scope->DeclareParameter(raw_name, VAR);
    ++(*num_params);
    return true;
  }

  // Case for more than one parameter:
  //   (foo, bar [, ...]) => ...
  if (expression->IsBinaryOperation()) {
    BinaryOperation* binop = expression->AsBinaryOperation();
    if (binop->op() != Token::COMMA || binop->left()->is_parenthesized() ||
        binop->right()->is_parenthesized())
      return false;

    return CheckAndDeclareArrowParameter(traits, binop->left(), scope,
                                         num_params, dupe_loc) &&
           CheckAndDeclareArrowParameter(traits, binop->right(), scope,
                                         num_params, dupe_loc);
  }

  // Any other kind of expression is not a valid parameter list.
  return false;
}


int ParserTraits::DeclareArrowParametersFromExpression(
    Expression* expression, Scope* scope, Scanner::Location* dupe_loc,
    bool* ok) {
  int num_params = 0;
  // Always reset the flag: It only needs to be set for the first expression
  // parsed as arrow function parameter list, becauseonly top-level functions
  // are parsed lazily.
  parser_->parsing_lazy_arrow_parameters_ = false;
  *ok = CheckAndDeclareArrowParameter(this, expression, scope, &num_params,
                                      dupe_loc);
  return num_params;
}


FunctionLiteral* Parser::ParseFunctionLiteral(
    const AstRawString* function_name, Scanner::Location function_name_location,
    bool name_is_strict_reserved, FunctionKind kind, int function_token_pos,
    FunctionLiteral::FunctionType function_type,
    FunctionLiteral::ArityRestriction arity_restriction, bool* ok) {
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

  int num_parameters = 0;
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
  Scope* scope = function_type == FunctionLiteral::DECLARATION &&
                         is_sloppy(language_mode()) &&
                         (original_scope_ == original_declaration_scope ||
                          declaration_scope != original_declaration_scope)
                     ? NewScope(declaration_scope, FUNCTION_SCOPE, kind)
                     : NewScope(scope_, FUNCTION_SCOPE, kind);
  ZoneList<Statement*>* body = NULL;
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  int handler_count = 0;
  FunctionLiteral::ParameterFlag duplicate_parameters =
      FunctionLiteral::kNoDuplicateParameters;
  FunctionLiteral::IsParenthesizedFlag parenthesized = parenthesized_function_
      ? FunctionLiteral::kIsParenthesized
      : FunctionLiteral::kNotParenthesized;
  // Parse function body.
  {
    AstNodeFactory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, scope, kind,
                                 &function_factory);
    scope_->SetScopeName(function_name);

    if (is_generator) {
      // For generators, allocating variables in contexts is currently a win
      // because it minimizes the work needed to suspend and resume an
      // activation.
      scope_->ForceContextAllocation();

      // Calling a generator returns a generator object.  That object is stored
      // in a temporary variable, a definition that is used by "yield"
      // expressions. This also marks the FunctionState as a generator.
      Variable* temp = scope_->DeclarationScope()->NewTemporary(
          ast_value_factory()->dot_generator_object_string());
      function_state.set_generator_object_variable(temp);
    }

    //  FormalParameterList ::
    //    '(' (Identifier)*[','] ')'
    Expect(Token::LPAREN, CHECK_OK);
    scope->set_start_position(scanner()->location().beg_pos);

    // We don't yet know if the function will be strict, so we cannot yet
    // produce errors for parameter names or duplicates. However, we remember
    // the locations of these errors if they occur and produce the errors later.
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

      const AstRawString* param_name =
          ParseIdentifierOrStrictReservedWord(&is_strict_reserved, CHECK_OK);

      // Store locations for possible future error reports.
      if (!eval_args_error_loc.IsValid() && IsEvalOrArguments(param_name)) {
        eval_args_error_loc = scanner()->location();
      }
      if (!reserved_error_loc.IsValid() && is_strict_reserved) {
        reserved_error_loc = scanner()->location();
      }
      if (!dupe_error_loc.IsValid() &&
          scope_->IsDeclaredParameter(param_name)) {
        duplicate_parameters = FunctionLiteral::kHasDuplicateParameters;
        dupe_error_loc = scanner()->location();
      }

      Variable* var = scope_->DeclareParameter(param_name, VAR, is_rest);
      if (is_sloppy(scope->language_mode())) {
        // TODO(sigurds) Mark every parameter as maybe assigned. This is a
        // conservative approximation necessary to account for parameters
        // that are assigned via the arguments array.
        var->set_maybe_assigned();
      }

      num_parameters++;
      if (num_parameters > Code::kMaxArguments) {
        ReportMessage("too_many_parameters");
        *ok = false;
        return NULL;
      }
      if (arity_restriction == FunctionLiteral::SETTER_ARITY) break;
      done = (peek() == Token::RPAREN);
      if (!done) {
        if (is_rest) {
          ReportMessageAt(scanner()->peek_location(), "param_after_rest");
          *ok = false;
          return NULL;
        }
        Expect(Token::COMMA, CHECK_OK);
      }
    }
    Expect(Token::RPAREN, CHECK_OK);

    Expect(Token::LBRACE, CHECK_OK);

    // If we have a named function expression, we add a local variable
    // declaration to the body of the function with the name of the
    // function and let it refer to the function itself (closure).
    // NOTE: We create a proxy and resolve it here so that in the
    // future we can change the AST to only refer to VariableProxies
    // instead of Variables and Proxis as is the case now.
    Variable* fvar = NULL;
    Token::Value fvar_init_op = Token::INIT_CONST_LEGACY;
    if (function_type == FunctionLiteral::NAMED_EXPRESSION) {
      if (is_strict(language_mode())) {
        fvar_init_op = Token::INIT_CONST;
      }
      VariableMode fvar_mode =
          is_strict(language_mode()) ? CONST : CONST_LEGACY;
      DCHECK(function_name != NULL);
      fvar = new (zone())
          Variable(scope_, function_name, fvar_mode, Variable::NORMAL,
                   kCreatedInitialized, kNotAssigned);
      VariableProxy* proxy = factory()->NewVariableProxy(fvar);
      VariableDeclaration* fvar_declaration = factory()->NewVariableDeclaration(
          proxy, fvar_mode, scope_, RelocInfo::kNoPosition);
      scope_->DeclareFunctionVar(fvar_declaration);
    }

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
    bool is_lazily_parsed = (mode() == PARSE_LAZILY &&
                             scope_->AllowsLazyCompilation() &&
                             !parenthesized_function_);
    parenthesized_function_ = false;  // The bit was set for this function only.

    if (is_lazily_parsed) {
      SkipLazyFunctionBody(function_name, &materialized_literal_count,
                           &expected_property_count, CHECK_OK);
    } else {
      body = ParseEagerFunctionBody(function_name, pos, fvar, fvar_init_op,
                                    kind, CHECK_OK);
      materialized_literal_count = function_state.materialized_literal_count();
      expected_property_count = function_state.expected_property_count();
      handler_count = function_state.handler_count();
    }

    // Validate name and parameter names. We can do this only after parsing the
    // function, since the function can declare itself strict.
    CheckFunctionName(language_mode(), kind, function_name,
                      name_is_strict_reserved, function_name_location,
                      CHECK_OK);
    const bool use_strict_params = is_rest || IsConciseMethod(kind);
    CheckFunctionParameterNames(language_mode(), use_strict_params,
                                eval_args_error_loc, dupe_error_loc,
                                reserved_error_loc, CHECK_OK);

    if (is_strict(language_mode())) {
      CheckStrictOctalLiteral(scope->start_position(), scope->end_position(),
                              CHECK_OK);
    }
    if (is_strict(language_mode())) {
      CheckConflictingVarDeclarations(scope, CHECK_OK);
    }
    if (is_strong(language_mode()) && IsSubclassConstructor(kind)) {
      if (!function_state.super_call_location().IsValid()) {
        ReportMessageAt(function_name_location, "strong_super_call_missing",
                        kReferenceError);
        *ok = false;
        return nullptr;
      }
    }
  }

  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      function_name, ast_value_factory(), scope, body,
      materialized_literal_count, expected_property_count, handler_count,
      num_parameters, duplicate_parameters, function_type,
      FunctionLiteral::kIsFunction, parenthesized, kind, pos);
  function_literal->set_function_token_position(function_token_pos);

  if (scope->has_rest_parameter()) {
    // TODO(caitp): enable optimization of functions with rest params
    function_literal->set_dont_optimize_reason(kRestParameter);
  }

  if (fni_ != NULL && should_infer_name) fni_->AddFunction(function_literal);
  return function_literal;
}


void Parser::SkipLazyFunctionBody(const AstRawString* function_name,
                                  int* materialized_literal_count,
                                  int* expected_property_count,
                                  bool* ok) {
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
      scope_->SetLanguageMode(entry.language_mode());
      if (entry.uses_super_property()) scope_->RecordSuperPropertyUsage();
      return;
    }
    cached_parse_data_->Reject();
  }
  // With no cached data, we partially parse the function, without building an
  // AST. This gathers the data needed to build a lazy function.
  SingletonLogger logger;
  PreParser::PreParseResult result =
      ParseLazyFunctionBodyWithPreParser(&logger);
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
  scope_->SetLanguageMode(logger.language_mode());
  if (logger.scope_uses_super_property()) {
    scope_->RecordSuperPropertyUsage();
  }
  if (produce_cached_parse_data()) {
    DCHECK(log_);
    // Position right after terminal '}'.
    int body_end = scanner()->location().end_pos;
    log_->LogFunction(function_block_pos, body_end, *materialized_literal_count,
                      *expected_property_count, scope_->language_mode(),
                      scope_->uses_super_property());
  }
}


void Parser::AddAssertIsConstruct(ZoneList<Statement*>* body, int pos) {
  ZoneList<Expression*>* arguments =
      new (zone()) ZoneList<Expression*>(0, zone());
  CallRuntime* construct_check = factory()->NewCallRuntime(
      ast_value_factory()->is_construct_call_string(),
      Runtime::FunctionForId(Runtime::kInlineIsConstructCall), arguments, pos);
  CallRuntime* non_callable_error = factory()->NewCallRuntime(
      ast_value_factory()->empty_string(),
      Runtime::FunctionForId(Runtime::kThrowConstructorNonCallableError),
      arguments, pos);
  IfStatement* if_statement = factory()->NewIfStatement(
      factory()->NewUnaryOperation(Token::NOT, construct_check, pos),
      factory()->NewReturnStatement(non_callable_error, pos),
      factory()->NewEmptyStatement(pos), pos);
  body->Add(if_statement, zone());
}


ZoneList<Statement*>* Parser::ParseEagerFunctionBody(
    const AstRawString* function_name, int pos, Variable* fvar,
    Token::Value fvar_init_op, FunctionKind kind, bool* ok) {
  // Everything inside an eagerly parsed function will be parsed eagerly
  // (see comment above).
  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);
  ZoneList<Statement*>* body = new(zone()) ZoneList<Statement*>(8, zone());
  if (fvar != NULL) {
    VariableProxy* fproxy = scope_->NewUnresolved(factory(), function_name);
    fproxy->BindTo(fvar);
    body->Add(factory()->NewExpressionStatement(
        factory()->NewAssignment(fvar_init_op,
                                 fproxy,
                                 factory()->NewThisFunction(pos),
                                 RelocInfo::kNoPosition),
        RelocInfo::kNoPosition), zone());
  }


  // For concise constructors, check that they are constructed,
  // not called.
  if (i::IsConstructor(kind)) {
    AddAssertIsConstruct(body, pos);
  }

  // For generators, allocate and yield an iterator on function entry.
  if (IsGeneratorFunction(kind)) {
    ZoneList<Expression*>* arguments =
        new(zone()) ZoneList<Expression*>(0, zone());
    CallRuntime* allocation = factory()->NewCallRuntime(
        ast_value_factory()->empty_string(),
        Runtime::FunctionForId(Runtime::kCreateJSGeneratorObject), arguments,
        pos);
    VariableProxy* init_proxy = factory()->NewVariableProxy(
        function_state_->generator_object_variable());
    Assignment* assignment = factory()->NewAssignment(
        Token::INIT_VAR, init_proxy, allocation, RelocInfo::kNoPosition);
    VariableProxy* get_proxy = factory()->NewVariableProxy(
        function_state_->generator_object_variable());
    Yield* yield = factory()->NewYield(
        get_proxy, assignment, Yield::kInitial, RelocInfo::kNoPosition);
    body->Add(factory()->NewExpressionStatement(
        yield, RelocInfo::kNoPosition), zone());
  }

  ParseStatementList(body, Token::RBRACE, false, NULL, CHECK_OK);

  if (IsGeneratorFunction(kind)) {
    VariableProxy* get_proxy = factory()->NewVariableProxy(
        function_state_->generator_object_variable());
    Expression* undefined =
        factory()->NewUndefinedLiteral(RelocInfo::kNoPosition);
    Yield* yield = factory()->NewYield(get_proxy, undefined, Yield::kFinal,
                                       RelocInfo::kNoPosition);
    body->Add(factory()->NewExpressionStatement(
        yield, RelocInfo::kNoPosition), zone());
  }

  if (IsSubclassConstructor(kind)) {
    body->Add(
        factory()->NewReturnStatement(
            this->ThisExpression(scope_, factory(), RelocInfo::kNoPosition),
            RelocInfo::kNoPosition),
        zone());
  }

  Expect(Token::RBRACE, CHECK_OK);
  scope_->set_end_position(scanner()->location().end_pos);

  return body;
}


PreParser::PreParseResult Parser::ParseLazyFunctionBodyWithPreParser(
    SingletonLogger* logger) {
  // This function may be called on a background thread too; record only the
  // main thread preparse times.
  if (pre_parse_timer_ != NULL) {
    pre_parse_timer_->Start();
  }
  DCHECK_EQ(Token::LBRACE, scanner()->current_token());

  if (reusable_preparser_ == NULL) {
    reusable_preparser_ = new PreParser(zone(), &scanner_, ast_value_factory(),
                                        NULL, stack_limit_);
    reusable_preparser_->set_allow_lazy(true);
    reusable_preparser_->set_allow_natives(allow_natives());
    reusable_preparser_->set_allow_harmony_modules(allow_harmony_modules());
    reusable_preparser_->set_allow_harmony_arrow_functions(
        allow_harmony_arrow_functions());
    reusable_preparser_->set_allow_harmony_numeric_literals(
        allow_harmony_numeric_literals());
    reusable_preparser_->set_allow_harmony_classes(allow_harmony_classes());
    reusable_preparser_->set_allow_harmony_object_literals(
        allow_harmony_object_literals());
    reusable_preparser_->set_allow_harmony_sloppy(allow_harmony_sloppy());
    reusable_preparser_->set_allow_harmony_unicode(allow_harmony_unicode());
    reusable_preparser_->set_allow_harmony_computed_property_names(
        allow_harmony_computed_property_names());
    reusable_preparser_->set_allow_harmony_rest_params(
        allow_harmony_rest_params());
    reusable_preparser_->set_allow_strong_mode(allow_strong_mode());
  }
  PreParser::PreParseResult result = reusable_preparser_->PreParseLazyFunction(
      language_mode(), function_state_->kind(), logger);
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
    ReportMessageAt(class_name_location, "unexpected_strict_reserved");
    *ok = false;
    return NULL;
  }
  if (IsEvalOrArguments(name)) {
    ReportMessageAt(class_name_location, "strict_eval_arguments");
    *ok = false;
    return NULL;
  }

  // Create a block scope which is additionally tagged as class scope; this is
  // important for resolving variable references to the class name in the strong
  // mode.
  Scope* block_scope = NewScope(scope_, BLOCK_SCOPE);
  block_scope->tag_as_class_scope();
  BlockState block_state(&scope_, block_scope);
  scope_->SetLanguageMode(
      static_cast<LanguageMode>(scope_->language_mode() | STRICT_BIT));
  scope_->SetScopeName(name);

  VariableProxy* proxy = NULL;
  if (name != NULL) {
    proxy = NewUnresolved(name, CONST);
    Declaration* declaration =
        factory()->NewVariableDeclaration(proxy, CONST, block_scope, pos);
    Declare(declaration, true, CHECK_OK);
  }

  Expression* extends = NULL;
  if (Check(Token::EXTENDS)) {
    block_scope->set_start_position(scanner()->location().end_pos);
    extends = ParseLeftHandSideExpression(CHECK_OK);
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
    if (fni_ != NULL) fni_->Enter();
    const bool in_class = true;
    const bool is_static = false;
    bool is_computed_name = false;  // Classes do not care about computed
                                    // property names here.
    ObjectLiteral::Property* property = ParsePropertyDefinition(
        &checker, in_class, has_extends, is_static, &is_computed_name,
        &has_seen_constructor, CHECK_OK);

    if (has_seen_constructor && constructor == NULL) {
      constructor = GetPropertyValue(property)->AsFunctionLiteral();
      DCHECK_NOT_NULL(constructor);
    } else {
      properties->Add(property, zone());
    }

    if (fni_ != NULL) {
      fni_->Infer();
      fni_->Leave();
    }
  }

  Expect(Token::RBRACE, CHECK_OK);
  int end_pos = scanner()->location().end_pos;

  if (constructor == NULL) {
    constructor =
        DefaultConstructor(extends != NULL, block_scope, pos, end_pos);
  }

  block_scope->set_end_position(end_pos);

  if (name != NULL) {
    DCHECK_NOT_NULL(proxy);
    proxy->var()->set_initializer_position(end_pos);
  } else {
    // Unnamed classes should not have scopes (the scope will be empty).
    DCHECK_EQ(block_scope->num_var_or_const(), 0);
    block_scope = nullptr;
  }

  return factory()->NewClassLiteral(name, block_scope, proxy, extends,
                                    constructor, properties, pos, end_pos);
}


Expression* Parser::ParseV8Intrinsic(bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments

  int pos = peek_position();
  Expect(Token::MOD, CHECK_OK);
  // Allow "eval" or "arguments" for backward compatibility.
  const AstRawString* name = ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
  ZoneList<Expression*>* args = ParseArguments(CHECK_OK);

  if (extension_ != NULL) {
    // The extension structures are only accessible while parsing the
    // very first time not when reparsing because of lazy compilation.
    scope_->DeclarationScope()->ForceEagerCompilation();
  }

  const Runtime::Function* function = Runtime::FunctionForName(name->string());

  // Check for built-in IS_VAR macro.
  if (function != NULL &&
      function->intrinsic_type == Runtime::RUNTIME &&
      function->function_id == Runtime::kIS_VAR) {
    // %IS_VAR(x) evaluates to x if x is a variable,
    // leads to a parse error otherwise.  Could be implemented as an
    // inline function %_IS_VAR(x) to eliminate this special case.
    if (args->length() == 1 && args->at(0)->AsVariableProxy() != NULL) {
      return args->at(0);
    } else {
      ReportMessage("not_isvar");
      *ok = false;
      return NULL;
    }
  }

  // Check that the expected number of arguments are being passed.
  if (function != NULL &&
      function->nargs != -1 &&
      function->nargs != args->length()) {
    ReportMessage("illegal_access");
    *ok = false;
    return NULL;
  }

  // Check that the function is defined if it's an inline runtime call.
  if (function == NULL && name->FirstCharacter() == '_') {
    ParserTraits::ReportMessage("not_defined", name);
    *ok = false;
    return NULL;
  }

  // We have a valid intrinsics call or a call to a builtin.
  return factory()->NewCallRuntime(name, function, args, pos);
}


Literal* Parser::GetLiteralUndefined(int position) {
  return factory()->NewUndefinedLiteral(position);
}


void Parser::CheckConflictingVarDeclarations(Scope* scope, bool* ok) {
  Declaration* decl = scope->CheckConflictingVarDeclarations();
  if (decl != NULL) {
    // In harmony mode we treat conflicting variable bindinds as early
    // errors. See ES5 16 for a definition of early errors.
    const AstRawString* name = decl->proxy()->raw_name();
    int position = decl->proxy()->position();
    Scanner::Location location = position == RelocInfo::kNoPosition
        ? Scanner::Location::invalid()
        : Scanner::Location(position, position + 1);
    ParserTraits::ReportMessageAt(location, "var_redeclaration", name);
    *ok = false;
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
  isolate->counters()->total_preparse_skipped()->Increment(
      total_preparse_skipped_);
}


// ----------------------------------------------------------------------------
// Regular expressions


RegExpParser::RegExpParser(FlatStringReader* in, Handle<String>* error,
                           bool multiline, bool unicode, Isolate* isolate,
                           Zone* zone)
    : isolate_(isolate),
      zone_(zone),
      error_(error),
      captures_(NULL),
      in_(in),
      current_(kEndMarker),
      next_pos_(0),
      capture_count_(0),
      has_more_(true),
      multiline_(multiline),
      unicode_(unicode),
      simple_(false),
      contains_anchor_(false),
      is_scanned_for_captures_(false),
      failed_(false) {
  Advance();
}


uc32 RegExpParser::Next() {
  if (has_next()) {
    return in()->Get(next_pos_);
  } else {
    return kEndMarker;
  }
}


void RegExpParser::Advance() {
  if (next_pos_ < in()->length()) {
    StackLimitCheck check(isolate());
    if (check.HasOverflowed()) {
      ReportError(CStrVector(Isolate::kStackOverflowMessage));
    } else if (zone()->excess_allocation()) {
      ReportError(CStrVector("Regular expression too large"));
    } else {
      current_ = in()->Get(next_pos_);
      next_pos_++;
    }
  } else {
    current_ = kEndMarker;
    // Advance so that position() points to 1-after-the-last-character. This is
    // important so that Reset() to this position works correctly.
    next_pos_ = in()->length() + 1;
    has_more_ = false;
  }
}


void RegExpParser::Reset(int pos) {
  next_pos_ = pos;
  has_more_ = (pos < in()->length());
  Advance();
}


void RegExpParser::Advance(int dist) {
  next_pos_ += dist - 1;
  Advance();
}


bool RegExpParser::simple() {
  return simple_;
}


bool RegExpParser::IsSyntaxCharacter(uc32 c) {
  return c == '^' || c == '$' || c == '\\' || c == '.' || c == '*' ||
         c == '+' || c == '?' || c == '(' || c == ')' || c == '[' || c == ']' ||
         c == '{' || c == '}' || c == '|';
}


RegExpTree* RegExpParser::ReportError(Vector<const char> message) {
  failed_ = true;
  *error_ = isolate()->factory()->NewStringFromAscii(message).ToHandleChecked();
  // Zip to the end to make sure the no more input is read.
  current_ = kEndMarker;
  next_pos_ = in()->length();
  return NULL;
}


// Pattern ::
//   Disjunction
RegExpTree* RegExpParser::ParsePattern() {
  RegExpTree* result = ParseDisjunction(CHECK_FAILED);
  DCHECK(!has_more());
  // If the result of parsing is a literal string atom, and it has the
  // same length as the input, then the atom is identical to the input.
  if (result->IsAtom() && result->AsAtom()->length() == in()->length()) {
    simple_ = true;
  }
  return result;
}


// Disjunction ::
//   Alternative
//   Alternative | Disjunction
// Alternative ::
//   [empty]
//   Term Alternative
// Term ::
//   Assertion
//   Atom
//   Atom Quantifier
RegExpTree* RegExpParser::ParseDisjunction() {
  // Used to store current state while parsing subexpressions.
  RegExpParserState initial_state(NULL, INITIAL, 0, zone());
  RegExpParserState* stored_state = &initial_state;
  // Cache the builder in a local variable for quick access.
  RegExpBuilder* builder = initial_state.builder();
  while (true) {
    switch (current()) {
    case kEndMarker:
      if (stored_state->IsSubexpression()) {
        // Inside a parenthesized group when hitting end of input.
        ReportError(CStrVector("Unterminated group") CHECK_FAILED);
      }
      DCHECK_EQ(INITIAL, stored_state->group_type());
      // Parsing completed successfully.
      return builder->ToRegExp();
    case ')': {
      if (!stored_state->IsSubexpression()) {
        ReportError(CStrVector("Unmatched ')'") CHECK_FAILED);
      }
      DCHECK_NE(INITIAL, stored_state->group_type());

      Advance();
      // End disjunction parsing and convert builder content to new single
      // regexp atom.
      RegExpTree* body = builder->ToRegExp();

      int end_capture_index = captures_started();

      int capture_index = stored_state->capture_index();
      SubexpressionType group_type = stored_state->group_type();

      // Restore previous state.
      stored_state = stored_state->previous_state();
      builder = stored_state->builder();

      // Build result of subexpression.
      if (group_type == CAPTURE) {
        RegExpCapture* capture = new(zone()) RegExpCapture(body, capture_index);
        captures_->at(capture_index - 1) = capture;
        body = capture;
      } else if (group_type != GROUPING) {
        DCHECK(group_type == POSITIVE_LOOKAHEAD ||
               group_type == NEGATIVE_LOOKAHEAD);
        bool is_positive = (group_type == POSITIVE_LOOKAHEAD);
        body = new(zone()) RegExpLookahead(body,
                                   is_positive,
                                   end_capture_index - capture_index,
                                   capture_index);
      }
      builder->AddAtom(body);
      // For compatability with JSC and ES3, we allow quantifiers after
      // lookaheads, and break in all cases.
      break;
    }
    case '|': {
      Advance();
      builder->NewAlternative();
      continue;
    }
    case '*':
    case '+':
    case '?':
      return ReportError(CStrVector("Nothing to repeat"));
    case '^': {
      Advance();
      if (multiline_) {
        builder->AddAssertion(
            new(zone()) RegExpAssertion(RegExpAssertion::START_OF_LINE));
      } else {
        builder->AddAssertion(
            new(zone()) RegExpAssertion(RegExpAssertion::START_OF_INPUT));
        set_contains_anchor();
      }
      continue;
    }
    case '$': {
      Advance();
      RegExpAssertion::AssertionType assertion_type =
          multiline_ ? RegExpAssertion::END_OF_LINE :
                       RegExpAssertion::END_OF_INPUT;
      builder->AddAssertion(new(zone()) RegExpAssertion(assertion_type));
      continue;
    }
    case '.': {
      Advance();
      // everything except \x0a, \x0d, \u2028 and \u2029
      ZoneList<CharacterRange>* ranges =
          new(zone()) ZoneList<CharacterRange>(2, zone());
      CharacterRange::AddClassEscape('.', ranges, zone());
      RegExpTree* atom = new(zone()) RegExpCharacterClass(ranges, false);
      builder->AddAtom(atom);
      break;
    }
    case '(': {
      SubexpressionType subexpr_type = CAPTURE;
      Advance();
      if (current() == '?') {
        switch (Next()) {
          case ':':
            subexpr_type = GROUPING;
            break;
          case '=':
            subexpr_type = POSITIVE_LOOKAHEAD;
            break;
          case '!':
            subexpr_type = NEGATIVE_LOOKAHEAD;
            break;
          default:
            ReportError(CStrVector("Invalid group") CHECK_FAILED);
            break;
        }
        Advance(2);
      } else {
        if (captures_ == NULL) {
          captures_ = new(zone()) ZoneList<RegExpCapture*>(2, zone());
        }
        if (captures_started() >= kMaxCaptures) {
          ReportError(CStrVector("Too many captures") CHECK_FAILED);
        }
        captures_->Add(NULL, zone());
      }
      // Store current state and begin new disjunction parsing.
      stored_state = new(zone()) RegExpParserState(stored_state, subexpr_type,
                                                   captures_started(), zone());
      builder = stored_state->builder();
      continue;
    }
    case '[': {
      RegExpTree* atom = ParseCharacterClass(CHECK_FAILED);
      builder->AddAtom(atom);
      break;
    }
    // Atom ::
    //   \ AtomEscape
    case '\\':
      switch (Next()) {
      case kEndMarker:
        return ReportError(CStrVector("\\ at end of pattern"));
      case 'b':
        Advance(2);
        builder->AddAssertion(
            new(zone()) RegExpAssertion(RegExpAssertion::BOUNDARY));
        continue;
      case 'B':
        Advance(2);
        builder->AddAssertion(
            new(zone()) RegExpAssertion(RegExpAssertion::NON_BOUNDARY));
        continue;
      // AtomEscape ::
      //   CharacterClassEscape
      //
      // CharacterClassEscape :: one of
      //   d D s S w W
      case 'd': case 'D': case 's': case 'S': case 'w': case 'W': {
        uc32 c = Next();
        Advance(2);
        ZoneList<CharacterRange>* ranges =
            new(zone()) ZoneList<CharacterRange>(2, zone());
        CharacterRange::AddClassEscape(c, ranges, zone());
        RegExpTree* atom = new(zone()) RegExpCharacterClass(ranges, false);
        builder->AddAtom(atom);
        break;
      }
      case '1': case '2': case '3': case '4': case '5': case '6':
      case '7': case '8': case '9': {
        int index = 0;
        if (ParseBackReferenceIndex(&index)) {
          RegExpCapture* capture = NULL;
          if (captures_ != NULL && index <= captures_->length()) {
            capture = captures_->at(index - 1);
          }
          if (capture == NULL) {
            builder->AddEmpty();
            break;
          }
          RegExpTree* atom = new(zone()) RegExpBackReference(capture);
          builder->AddAtom(atom);
          break;
        }
        uc32 first_digit = Next();
        if (first_digit == '8' || first_digit == '9') {
          // If the 'u' flag is present, only syntax characters can be escaped,
          // no other identity escapes are allowed. If the 'u' flag is not
          // present, all identity escapes are allowed.
          if (!FLAG_harmony_unicode_regexps || !unicode_) {
            builder->AddCharacter(first_digit);
            Advance(2);
          } else {
            return ReportError(CStrVector("Invalid escape"));
          }
          break;
        }
      }
      // FALLTHROUGH
      case '0': {
        Advance();
        uc32 octal = ParseOctalLiteral();
        builder->AddCharacter(octal);
        break;
      }
      // ControlEscape :: one of
      //   f n r t v
      case 'f':
        Advance(2);
        builder->AddCharacter('\f');
        break;
      case 'n':
        Advance(2);
        builder->AddCharacter('\n');
        break;
      case 'r':
        Advance(2);
        builder->AddCharacter('\r');
        break;
      case 't':
        Advance(2);
        builder->AddCharacter('\t');
        break;
      case 'v':
        Advance(2);
        builder->AddCharacter('\v');
        break;
      case 'c': {
        Advance();
        uc32 controlLetter = Next();
        // Special case if it is an ASCII letter.
        // Convert lower case letters to uppercase.
        uc32 letter = controlLetter & ~('a' ^ 'A');
        if (letter < 'A' || 'Z' < letter) {
          // controlLetter is not in range 'A'-'Z' or 'a'-'z'.
          // This is outside the specification. We match JSC in
          // reading the backslash as a literal character instead
          // of as starting an escape.
          builder->AddCharacter('\\');
        } else {
          Advance(2);
          builder->AddCharacter(controlLetter & 0x1f);
        }
        break;
      }
      case 'x': {
        Advance(2);
        uc32 value;
        if (ParseHexEscape(2, &value)) {
          builder->AddCharacter(value);
        } else if (!FLAG_harmony_unicode_regexps || !unicode_) {
          builder->AddCharacter('x');
        } else {
          // If the 'u' flag is present, invalid escapes are not treated as
          // identity escapes.
          return ReportError(CStrVector("Invalid escape"));
        }
        break;
      }
      case 'u': {
        Advance(2);
        uc32 value;
        if (ParseUnicodeEscape(&value)) {
          builder->AddCharacter(value);
        } else if (!FLAG_harmony_unicode_regexps || !unicode_) {
          builder->AddCharacter('u');
        } else {
          // If the 'u' flag is present, invalid escapes are not treated as
          // identity escapes.
          return ReportError(CStrVector("Invalid unicode escape"));
        }
        break;
      }
      default:
        Advance();
        // If the 'u' flag is present, only syntax characters can be escaped, no
        // other identity escapes are allowed. If the 'u' flag is not present,
        // all identity escapes are allowed.
        if (!FLAG_harmony_unicode_regexps || !unicode_ ||
            IsSyntaxCharacter(current())) {
          builder->AddCharacter(current());
          Advance();
        } else {
          return ReportError(CStrVector("Invalid escape"));
        }
        break;
      }
      break;
    case '{': {
      int dummy;
      if (ParseIntervalQuantifier(&dummy, &dummy)) {
        ReportError(CStrVector("Nothing to repeat") CHECK_FAILED);
      }
      // fallthrough
    }
    default:
      builder->AddCharacter(current());
      Advance();
      break;
    }  // end switch(current())

    int min;
    int max;
    switch (current()) {
    // QuantifierPrefix ::
    //   *
    //   +
    //   ?
    //   {
    case '*':
      min = 0;
      max = RegExpTree::kInfinity;
      Advance();
      break;
    case '+':
      min = 1;
      max = RegExpTree::kInfinity;
      Advance();
      break;
    case '?':
      min = 0;
      max = 1;
      Advance();
      break;
    case '{':
      if (ParseIntervalQuantifier(&min, &max)) {
        if (max < min) {
          ReportError(CStrVector("numbers out of order in {} quantifier.")
                      CHECK_FAILED);
        }
        break;
      } else {
        continue;
      }
    default:
      continue;
    }
    RegExpQuantifier::QuantifierType quantifier_type = RegExpQuantifier::GREEDY;
    if (current() == '?') {
      quantifier_type = RegExpQuantifier::NON_GREEDY;
      Advance();
    } else if (FLAG_regexp_possessive_quantifier && current() == '+') {
      // FLAG_regexp_possessive_quantifier is a debug-only flag.
      quantifier_type = RegExpQuantifier::POSSESSIVE;
      Advance();
    }
    builder->AddQuantifierToAtom(min, max, quantifier_type);
  }
}


#ifdef DEBUG
// Currently only used in an DCHECK.
static bool IsSpecialClassEscape(uc32 c) {
  switch (c) {
    case 'd': case 'D':
    case 's': case 'S':
    case 'w': case 'W':
      return true;
    default:
      return false;
  }
}
#endif


// In order to know whether an escape is a backreference or not we have to scan
// the entire regexp and find the number of capturing parentheses.  However we
// don't want to scan the regexp twice unless it is necessary.  This mini-parser
// is called when needed.  It can see the difference between capturing and
// noncapturing parentheses and can skip character classes and backslash-escaped
// characters.
void RegExpParser::ScanForCaptures() {
  // Start with captures started previous to current position
  int capture_count = captures_started();
  // Add count of captures after this position.
  int n;
  while ((n = current()) != kEndMarker) {
    Advance();
    switch (n) {
      case '\\':
        Advance();
        break;
      case '[': {
        int c;
        while ((c = current()) != kEndMarker) {
          Advance();
          if (c == '\\') {
            Advance();
          } else {
            if (c == ']') break;
          }
        }
        break;
      }
      case '(':
        if (current() != '?') capture_count++;
        break;
    }
  }
  capture_count_ = capture_count;
  is_scanned_for_captures_ = true;
}


bool RegExpParser::ParseBackReferenceIndex(int* index_out) {
  DCHECK_EQ('\\', current());
  DCHECK('1' <= Next() && Next() <= '9');
  // Try to parse a decimal literal that is no greater than the total number
  // of left capturing parentheses in the input.
  int start = position();
  int value = Next() - '0';
  Advance(2);
  while (true) {
    uc32 c = current();
    if (IsDecimalDigit(c)) {
      value = 10 * value + (c - '0');
      if (value > kMaxCaptures) {
        Reset(start);
        return false;
      }
      Advance();
    } else {
      break;
    }
  }
  if (value > captures_started()) {
    if (!is_scanned_for_captures_) {
      int saved_position = position();
      ScanForCaptures();
      Reset(saved_position);
    }
    if (value > capture_count_) {
      Reset(start);
      return false;
    }
  }
  *index_out = value;
  return true;
}


// QuantifierPrefix ::
//   { DecimalDigits }
//   { DecimalDigits , }
//   { DecimalDigits , DecimalDigits }
//
// Returns true if parsing succeeds, and set the min_out and max_out
// values. Values are truncated to RegExpTree::kInfinity if they overflow.
bool RegExpParser::ParseIntervalQuantifier(int* min_out, int* max_out) {
  DCHECK_EQ(current(), '{');
  int start = position();
  Advance();
  int min = 0;
  if (!IsDecimalDigit(current())) {
    Reset(start);
    return false;
  }
  while (IsDecimalDigit(current())) {
    int next = current() - '0';
    if (min > (RegExpTree::kInfinity - next) / 10) {
      // Overflow. Skip past remaining decimal digits and return -1.
      do {
        Advance();
      } while (IsDecimalDigit(current()));
      min = RegExpTree::kInfinity;
      break;
    }
    min = 10 * min + next;
    Advance();
  }
  int max = 0;
  if (current() == '}') {
    max = min;
    Advance();
  } else if (current() == ',') {
    Advance();
    if (current() == '}') {
      max = RegExpTree::kInfinity;
      Advance();
    } else {
      while (IsDecimalDigit(current())) {
        int next = current() - '0';
        if (max > (RegExpTree::kInfinity - next) / 10) {
          do {
            Advance();
          } while (IsDecimalDigit(current()));
          max = RegExpTree::kInfinity;
          break;
        }
        max = 10 * max + next;
        Advance();
      }
      if (current() != '}') {
        Reset(start);
        return false;
      }
      Advance();
    }
  } else {
    Reset(start);
    return false;
  }
  *min_out = min;
  *max_out = max;
  return true;
}


uc32 RegExpParser::ParseOctalLiteral() {
  DCHECK(('0' <= current() && current() <= '7') || current() == kEndMarker);
  // For compatibility with some other browsers (not all), we parse
  // up to three octal digits with a value below 256.
  uc32 value = current() - '0';
  Advance();
  if ('0' <= current() && current() <= '7') {
    value = value * 8 + current() - '0';
    Advance();
    if (value < 32 && '0' <= current() && current() <= '7') {
      value = value * 8 + current() - '0';
      Advance();
    }
  }
  return value;
}


bool RegExpParser::ParseHexEscape(int length, uc32* value) {
  int start = position();
  uc32 val = 0;
  for (int i = 0; i < length; ++i) {
    uc32 c = current();
    int d = HexValue(c);
    if (d < 0) {
      Reset(start);
      return false;
    }
    val = val * 16 + d;
    Advance();
  }
  *value = val;
  return true;
}


bool RegExpParser::ParseUnicodeEscape(uc32* value) {
  // Accept both \uxxxx and \u{xxxxxx} (if harmony unicode escapes are
  // allowed). In the latter case, the number of hex digits between { } is
  // arbitrary. \ and u have already been read.
  if (current() == '{' && FLAG_harmony_unicode_regexps && unicode_) {
    int start = position();
    Advance();
    if (ParseUnlimitedLengthHexNumber(0x10ffff, value)) {
      if (current() == '}') {
        Advance();
        return true;
      }
    }
    Reset(start);
    return false;
  }
  // \u but no {, or \u{...} escapes not allowed.
  return ParseHexEscape(4, value);
}


bool RegExpParser::ParseUnlimitedLengthHexNumber(int max_value, uc32* value) {
  uc32 x = 0;
  int d = HexValue(current());
  if (d < 0) {
    return false;
  }
  while (d >= 0) {
    x = x * 16 + d;
    if (x > max_value) {
      return false;
    }
    Advance();
    d = HexValue(current());
  }
  *value = x;
  return true;
}


uc32 RegExpParser::ParseClassCharacterEscape() {
  DCHECK(current() == '\\');
  DCHECK(has_next() && !IsSpecialClassEscape(Next()));
  Advance();
  switch (current()) {
    case 'b':
      Advance();
      return '\b';
    // ControlEscape :: one of
    //   f n r t v
    case 'f':
      Advance();
      return '\f';
    case 'n':
      Advance();
      return '\n';
    case 'r':
      Advance();
      return '\r';
    case 't':
      Advance();
      return '\t';
    case 'v':
      Advance();
      return '\v';
    case 'c': {
      uc32 controlLetter = Next();
      uc32 letter = controlLetter & ~('A' ^ 'a');
      // For compatibility with JSC, inside a character class
      // we also accept digits and underscore as control characters.
      if ((controlLetter >= '0' && controlLetter <= '9') ||
          controlLetter == '_' ||
          (letter >= 'A' && letter <= 'Z')) {
        Advance(2);
        // Control letters mapped to ASCII control characters in the range
        // 0x00-0x1f.
        return controlLetter & 0x1f;
      }
      // We match JSC in reading the backslash as a literal
      // character instead of as starting an escape.
      return '\\';
    }
    case '0': case '1': case '2': case '3': case '4': case '5':
    case '6': case '7':
      // For compatibility, we interpret a decimal escape that isn't
      // a back reference (and therefore either \0 or not valid according
      // to the specification) as a 1..3 digit octal character code.
      return ParseOctalLiteral();
    case 'x': {
      Advance();
      uc32 value;
      if (ParseHexEscape(2, &value)) {
        return value;
      }
      if (!FLAG_harmony_unicode_regexps || !unicode_) {
        // If \x is not followed by a two-digit hexadecimal, treat it
        // as an identity escape.
        return 'x';
      }
      // If the 'u' flag is present, invalid escapes are not treated as
      // identity escapes.
      ReportError(CStrVector("Invalid escape"));
      return 0;
    }
    case 'u': {
      Advance();
      uc32 value;
      if (ParseUnicodeEscape(&value)) {
        return value;
      }
      if (!FLAG_harmony_unicode_regexps || !unicode_) {
        return 'u';
      }
      // If the 'u' flag is present, invalid escapes are not treated as
      // identity escapes.
      ReportError(CStrVector("Invalid unicode escape"));
      return 0;
    }
    default: {
      uc32 result = current();
      // If the 'u' flag is present, only syntax characters can be escaped, no
      // other identity escapes are allowed. If the 'u' flag is not present, all
      // identity escapes are allowed.
      if (!FLAG_harmony_unicode_regexps || !unicode_ ||
          IsSyntaxCharacter(result)) {
        Advance();
        return result;
      }
      ReportError(CStrVector("Invalid escape"));
      return 0;
    }
  }
  return 0;
}


CharacterRange RegExpParser::ParseClassAtom(uc16* char_class) {
  DCHECK_EQ(0, *char_class);
  uc32 first = current();
  if (first == '\\') {
    switch (Next()) {
      case 'w': case 'W': case 'd': case 'D': case 's': case 'S': {
        *char_class = Next();
        Advance(2);
        return CharacterRange::Singleton(0);  // Return dummy value.
      }
      case kEndMarker:
        return ReportError(CStrVector("\\ at end of pattern"));
      default:
        uc32 c = ParseClassCharacterEscape(CHECK_FAILED);
        return CharacterRange::Singleton(c);
    }
  } else {
    Advance();
    return CharacterRange::Singleton(first);
  }
}


static const uc16 kNoCharClass = 0;

// Adds range or pre-defined character class to character ranges.
// If char_class is not kInvalidClass, it's interpreted as a class
// escape (i.e., 's' means whitespace, from '\s').
static inline void AddRangeOrEscape(ZoneList<CharacterRange>* ranges,
                                    uc16 char_class,
                                    CharacterRange range,
                                    Zone* zone) {
  if (char_class != kNoCharClass) {
    CharacterRange::AddClassEscape(char_class, ranges, zone);
  } else {
    ranges->Add(range, zone);
  }
}


RegExpTree* RegExpParser::ParseCharacterClass() {
  static const char* kUnterminated = "Unterminated character class";
  static const char* kRangeOutOfOrder = "Range out of order in character class";

  DCHECK_EQ(current(), '[');
  Advance();
  bool is_negated = false;
  if (current() == '^') {
    is_negated = true;
    Advance();
  }
  ZoneList<CharacterRange>* ranges =
      new(zone()) ZoneList<CharacterRange>(2, zone());
  while (has_more() && current() != ']') {
    uc16 char_class = kNoCharClass;
    CharacterRange first = ParseClassAtom(&char_class CHECK_FAILED);
    if (current() == '-') {
      Advance();
      if (current() == kEndMarker) {
        // If we reach the end we break out of the loop and let the
        // following code report an error.
        break;
      } else if (current() == ']') {
        AddRangeOrEscape(ranges, char_class, first, zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        break;
      }
      uc16 char_class_2 = kNoCharClass;
      CharacterRange next = ParseClassAtom(&char_class_2 CHECK_FAILED);
      if (char_class != kNoCharClass || char_class_2 != kNoCharClass) {
        // Either end is an escaped character class. Treat the '-' verbatim.
        AddRangeOrEscape(ranges, char_class, first, zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        AddRangeOrEscape(ranges, char_class_2, next, zone());
        continue;
      }
      if (first.from() > next.to()) {
        return ReportError(CStrVector(kRangeOutOfOrder) CHECK_FAILED);
      }
      ranges->Add(CharacterRange::Range(first.from(), next.to()), zone());
    } else {
      AddRangeOrEscape(ranges, char_class, first, zone());
    }
  }
  if (!has_more()) {
    return ReportError(CStrVector(kUnterminated) CHECK_FAILED);
  }
  Advance();
  if (ranges->length() == 0) {
    ranges->Add(CharacterRange::Everything(), zone());
    is_negated = !is_negated;
  }
  return new(zone()) RegExpCharacterClass(ranges, is_negated);
}


// ----------------------------------------------------------------------------
// The Parser interface.

bool RegExpParser::ParseRegExp(Isolate* isolate, Zone* zone,
                               FlatStringReader* input, bool multiline,
                               bool unicode, RegExpCompileData* result) {
  DCHECK(result != NULL);
  RegExpParser parser(input, &result->error, multiline, unicode, isolate, zone);
  RegExpTree* tree = parser.ParsePattern();
  if (parser.failed()) {
    DCHECK(tree == NULL);
    DCHECK(!result->error.is_null());
  } else {
    DCHECK(tree != NULL);
    DCHECK(result->error.is_null());
    result->tree = tree;
    int capture_count = parser.captures_started();
    result->simple = tree->IsAtom() && parser.simple() && capture_count == 0;
    result->contains_anchor = parser.contains_anchor();
    result->capture_count = capture_count;
  }
  return !parser.failed();
}


bool Parser::ParseStatic(ParseInfo* info) {
  Parser parser(info);
  if (parser.Parse(info)) {
    info->set_language_mode(info->function()->language_mode());
    return true;
  }
  return false;
}


bool Parser::Parse(ParseInfo* info) {
  DCHECK(info->function() == NULL);
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

  DCHECK(info->function() == NULL);
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
  Scope* top_scope = NULL;
  Scope* eval_scope = NULL;
  result = DoParseProgram(info, &top_scope, &eval_scope);

  top_scope->set_end_position(scanner()->location().end_pos);
  if (eval_scope != NULL) {
    eval_scope->set_end_position(scanner()->location().end_pos);
  }

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
      Expression* middle = factory()->NewCallRuntime(
          ast_value_factory()->to_string_string(), NULL, args,
          sub->position());

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

    // GetTemplateCallSite
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

    this->CheckPossibleEvalCall(tag, scope_);
    Expression* call_site = factory()->NewCallRuntime(
        ast_value_factory()->get_template_callsite_string(), NULL, args, start);

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
} }  // namespace v8::internal
