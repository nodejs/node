// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "ast.h"
#include "parser.h"
#include "scopes.h"
#include "string-stream.h"
#include "type-info.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// All the Accept member functions for each syntax tree node type.

#define DECL_ACCEPT(type)                                       \
  void type::Accept(AstVisitor* v) { v->Visit##type(this); }
AST_NODE_LIST(DECL_ACCEPT)
#undef DECL_ACCEPT


// ----------------------------------------------------------------------------
// Implementation of other node functionality.

Assignment* ExpressionStatement::StatementAsSimpleAssignment() {
  return (expression()->AsAssignment() != NULL &&
          !expression()->AsAssignment()->is_compound())
      ? expression()->AsAssignment()
      : NULL;
}


CountOperation* ExpressionStatement::StatementAsCountOperation() {
  return expression()->AsCountOperation();
}


VariableProxy::VariableProxy(Isolate* isolate, Variable* var)
    : Expression(isolate),
      name_(var->name()),
      var_(NULL),  // Will be set by the call to BindTo.
      is_this_(var->is_this()),
      inside_with_(false),
      is_trivial_(false),
      position_(RelocInfo::kNoPosition) {
  BindTo(var);
}


VariableProxy::VariableProxy(Isolate* isolate,
                             Handle<String> name,
                             bool is_this,
                             bool inside_with,
                             int position)
    : Expression(isolate),
      name_(name),
      var_(NULL),
      is_this_(is_this),
      inside_with_(inside_with),
      is_trivial_(false),
      position_(position) {
  // Names must be canonicalized for fast equality checks.
  ASSERT(name->IsSymbol());
}


void VariableProxy::BindTo(Variable* var) {
  ASSERT(var_ == NULL);  // must be bound only once
  ASSERT(var != NULL);  // must bind
  ASSERT((is_this() && var->is_this()) || name_.is_identical_to(var->name()));
  // Ideally CONST-ness should match. However, this is very hard to achieve
  // because we don't know the exact semantics of conflicting (const and
  // non-const) multiple variable declarations, const vars introduced via
  // eval() etc.  Const-ness and variable declarations are a complete mess
  // in JS. Sigh...
  var_ = var;
  var->set_is_used(true);
}


Assignment::Assignment(Isolate* isolate,
                       Token::Value op,
                       Expression* target,
                       Expression* value,
                       int pos)
    : Expression(isolate),
      op_(op),
      target_(target),
      value_(value),
      pos_(pos),
      binary_operation_(NULL),
      compound_load_id_(kNoNumber),
      assignment_id_(GetNextId(isolate)),
      block_start_(false),
      block_end_(false),
      is_monomorphic_(false) {
  ASSERT(Token::IsAssignmentOp(op));
  if (is_compound()) {
    binary_operation_ =
        new(isolate->zone()) BinaryOperation(isolate,
                                             binary_op(),
                                             target,
                                             value,
                                             pos + 1);
    compound_load_id_ = GetNextId(isolate);
  }
}


Token::Value Assignment::binary_op() const {
  switch (op_) {
    case Token::ASSIGN_BIT_OR: return Token::BIT_OR;
    case Token::ASSIGN_BIT_XOR: return Token::BIT_XOR;
    case Token::ASSIGN_BIT_AND: return Token::BIT_AND;
    case Token::ASSIGN_SHL: return Token::SHL;
    case Token::ASSIGN_SAR: return Token::SAR;
    case Token::ASSIGN_SHR: return Token::SHR;
    case Token::ASSIGN_ADD: return Token::ADD;
    case Token::ASSIGN_SUB: return Token::SUB;
    case Token::ASSIGN_MUL: return Token::MUL;
    case Token::ASSIGN_DIV: return Token::DIV;
    case Token::ASSIGN_MOD: return Token::MOD;
    default: UNREACHABLE();
  }
  return Token::ILLEGAL;
}


bool FunctionLiteral::AllowsLazyCompilation() {
  return scope()->AllowsLazyCompilation();
}


ObjectLiteral::Property::Property(Literal* key, Expression* value) {
  emit_store_ = true;
  key_ = key;
  value_ = value;
  Object* k = *key->handle();
  if (k->IsSymbol() && HEAP->Proto_symbol()->Equals(String::cast(k))) {
    kind_ = PROTOTYPE;
  } else if (value_->AsMaterializedLiteral() != NULL) {
    kind_ = MATERIALIZED_LITERAL;
  } else if (value_->AsLiteral() != NULL) {
    kind_ = CONSTANT;
  } else {
    kind_ = COMPUTED;
  }
}


ObjectLiteral::Property::Property(bool is_getter, FunctionLiteral* value) {
  Isolate* isolate = Isolate::Current();
  emit_store_ = true;
  key_ = new(isolate->zone()) Literal(isolate, value->name());
  value_ = value;
  kind_ = is_getter ? GETTER : SETTER;
}


bool ObjectLiteral::Property::IsCompileTimeValue() {
  return kind_ == CONSTANT ||
      (kind_ == MATERIALIZED_LITERAL &&
       CompileTimeValue::IsCompileTimeValue(value_));
}


void ObjectLiteral::Property::set_emit_store(bool emit_store) {
  emit_store_ = emit_store;
}


bool ObjectLiteral::Property::emit_store() {
  return emit_store_;
}


bool IsEqualString(void* first, void* second) {
  ASSERT((*reinterpret_cast<String**>(first))->IsString());
  ASSERT((*reinterpret_cast<String**>(second))->IsString());
  Handle<String> h1(reinterpret_cast<String**>(first));
  Handle<String> h2(reinterpret_cast<String**>(second));
  return (*h1)->Equals(*h2);
}


bool IsEqualNumber(void* first, void* second) {
  ASSERT((*reinterpret_cast<Object**>(first))->IsNumber());
  ASSERT((*reinterpret_cast<Object**>(second))->IsNumber());

  Handle<Object> h1(reinterpret_cast<Object**>(first));
  Handle<Object> h2(reinterpret_cast<Object**>(second));
  if (h1->IsSmi()) {
    return h2->IsSmi() && *h1 == *h2;
  }
  if (h2->IsSmi()) return false;
  Handle<HeapNumber> n1 = Handle<HeapNumber>::cast(h1);
  Handle<HeapNumber> n2 = Handle<HeapNumber>::cast(h2);
  ASSERT(isfinite(n1->value()));
  ASSERT(isfinite(n2->value()));
  return n1->value() == n2->value();
}


void ObjectLiteral::CalculateEmitStore() {
  HashMap properties(&IsEqualString);
  HashMap elements(&IsEqualNumber);
  for (int i = this->properties()->length() - 1; i >= 0; i--) {
    ObjectLiteral::Property* property = this->properties()->at(i);
    Literal* literal = property->key();
    Handle<Object> handle = literal->handle();

    if (handle->IsNull()) {
      continue;
    }

    uint32_t hash;
    HashMap* table;
    void* key;
    Factory* factory = Isolate::Current()->factory();
    if (handle->IsSymbol()) {
      Handle<String> name(String::cast(*handle));
      if (name->AsArrayIndex(&hash)) {
        Handle<Object> key_handle = factory->NewNumberFromUint(hash);
        key = key_handle.location();
        table = &elements;
      } else {
        key = name.location();
        hash = name->Hash();
        table = &properties;
      }
    } else if (handle->ToArrayIndex(&hash)) {
      key = handle.location();
      table = &elements;
    } else {
      ASSERT(handle->IsNumber());
      double num = handle->Number();
      char arr[100];
      Vector<char> buffer(arr, ARRAY_SIZE(arr));
      const char* str = DoubleToCString(num, buffer);
      Handle<String> name = factory->NewStringFromAscii(CStrVector(str));
      key = name.location();
      hash = name->Hash();
      table = &properties;
    }
    // If the key of a computed property is in the table, do not emit
    // a store for the property later.
    if (property->kind() == ObjectLiteral::Property::COMPUTED) {
      if (table->Lookup(key, hash, false) != NULL) {
        property->set_emit_store(false);
      }
    }
    // Add key to the table.
    table->Lookup(key, hash, true);
  }
}


void TargetCollector::AddTarget(Label* target) {
  // Add the label to the collector, but discard duplicates.
  int length = targets_.length();
  for (int i = 0; i < length; i++) {
    if (targets_[i] == target) return;
  }
  targets_.Add(target);
}


bool UnaryOperation::ResultOverwriteAllowed() {
  switch (op_) {
    case Token::BIT_NOT:
    case Token::SUB:
      return true;
    default:
      return false;
  }
}


bool BinaryOperation::ResultOverwriteAllowed() {
  switch (op_) {
    case Token::COMMA:
    case Token::OR:
    case Token::AND:
      return false;
    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      return true;
    default:
      UNREACHABLE();
  }
  return false;
}


bool CompareOperation::IsLiteralCompareTypeof(Expression** expr,
                                              Handle<String>* check) {
  if (op_ != Token::EQ && op_ != Token::EQ_STRICT) return false;

  UnaryOperation* left_unary = left_->AsUnaryOperation();
  UnaryOperation* right_unary = right_->AsUnaryOperation();
  Literal* left_literal = left_->AsLiteral();
  Literal* right_literal = right_->AsLiteral();

  // Check for the pattern: typeof <expression> == <string literal>.
  if (left_unary != NULL && left_unary->op() == Token::TYPEOF &&
      right_literal != NULL && right_literal->handle()->IsString()) {
    *expr = left_unary->expression();
    *check = Handle<String>::cast(right_literal->handle());
    return true;
  }

  // Check for the pattern: <string literal> == typeof <expression>.
  if (right_unary != NULL && right_unary->op() == Token::TYPEOF &&
      left_literal != NULL && left_literal->handle()->IsString()) {
    *expr = right_unary->expression();
    *check = Handle<String>::cast(left_literal->handle());
    return true;
  }

  return false;
}


bool CompareOperation::IsLiteralCompareUndefined(Expression** expr) {
  if (op_ != Token::EQ_STRICT) return false;

  UnaryOperation* left_unary = left_->AsUnaryOperation();
  UnaryOperation* right_unary = right_->AsUnaryOperation();

  // Check for the pattern: <expression> === void <literal>.
  if (right_unary != NULL && right_unary->op() == Token::VOID &&
      right_unary->expression()->AsLiteral() != NULL) {
    *expr = left_;
    return true;
  }

  // Check for the pattern: void <literal> === <expression>.
  if (left_unary != NULL && left_unary->op() == Token::VOID &&
      left_unary->expression()->AsLiteral() != NULL) {
    *expr = right_;
    return true;
  }

  return false;
}


// ----------------------------------------------------------------------------
// Inlining support

bool Declaration::IsInlineable() const {
  return proxy()->var()->IsStackAllocated() && fun() == NULL;
}


bool TargetCollector::IsInlineable() const {
  UNREACHABLE();
  return false;
}


bool ForInStatement::IsInlineable() const {
  return false;
}


bool WithStatement::IsInlineable() const {
  return false;
}


bool SwitchStatement::IsInlineable() const {
  return false;
}


bool TryStatement::IsInlineable() const {
  return false;
}


bool TryCatchStatement::IsInlineable() const {
  return false;
}


bool TryFinallyStatement::IsInlineable() const {
  return false;
}


bool DebuggerStatement::IsInlineable() const {
  return false;
}


bool Throw::IsInlineable() const {
  return exception()->IsInlineable();
}


bool MaterializedLiteral::IsInlineable() const {
  // TODO(1322): Allow materialized literals.
  return false;
}


bool FunctionLiteral::IsInlineable() const {
  // TODO(1322): Allow materialized literals.
  return false;
}


bool ThisFunction::IsInlineable() const {
  return false;
}


bool SharedFunctionInfoLiteral::IsInlineable() const {
  return false;
}


bool ForStatement::IsInlineable() const {
  return (init() == NULL || init()->IsInlineable())
      && (cond() == NULL || cond()->IsInlineable())
      && (next() == NULL || next()->IsInlineable())
      && body()->IsInlineable();
}


bool WhileStatement::IsInlineable() const {
  return cond()->IsInlineable()
      && body()->IsInlineable();
}


bool DoWhileStatement::IsInlineable() const {
  return cond()->IsInlineable()
      && body()->IsInlineable();
}


bool ContinueStatement::IsInlineable() const {
  return true;
}


bool BreakStatement::IsInlineable() const {
  return true;
}


bool EmptyStatement::IsInlineable() const {
  return true;
}


bool Literal::IsInlineable() const {
  return true;
}


bool Block::IsInlineable() const {
  const int count = statements_.length();
  for (int i = 0; i < count; ++i) {
    if (!statements_[i]->IsInlineable()) return false;
  }
  return true;
}


bool ExpressionStatement::IsInlineable() const {
  return expression()->IsInlineable();
}


bool IfStatement::IsInlineable() const {
  return condition()->IsInlineable()
      && then_statement()->IsInlineable()
      && else_statement()->IsInlineable();
}


bool ReturnStatement::IsInlineable() const {
  return expression()->IsInlineable();
}


bool Conditional::IsInlineable() const {
  return condition()->IsInlineable() && then_expression()->IsInlineable() &&
      else_expression()->IsInlineable();
}


bool VariableProxy::IsInlineable() const {
  return var()->IsUnallocated() || var()->IsStackAllocated();
}


bool Assignment::IsInlineable() const {
  return target()->IsInlineable() && value()->IsInlineable();
}


bool Property::IsInlineable() const {
  return obj()->IsInlineable() && key()->IsInlineable();
}


bool Call::IsInlineable() const {
  if (!expression()->IsInlineable()) return false;
  const int count = arguments()->length();
  for (int i = 0; i < count; ++i) {
    if (!arguments()->at(i)->IsInlineable()) return false;
  }
  return true;
}


bool CallNew::IsInlineable() const {
  if (!expression()->IsInlineable()) return false;
  const int count = arguments()->length();
  for (int i = 0; i < count; ++i) {
    if (!arguments()->at(i)->IsInlineable()) return false;
  }
  return true;
}


bool CallRuntime::IsInlineable() const {
  // Don't try to inline JS runtime calls because we don't (currently) even
  // optimize them.
  if (is_jsruntime()) return false;
  // Don't inline the %_ArgumentsLength or %_Arguments because their
  // implementation will not work.  There is no stack frame to get them
  // from.
  if (function()->intrinsic_type == Runtime::INLINE &&
      (name()->IsEqualTo(CStrVector("_ArgumentsLength")) ||
       name()->IsEqualTo(CStrVector("_Arguments")))) {
    return false;
  }
  const int count = arguments()->length();
  for (int i = 0; i < count; ++i) {
    if (!arguments()->at(i)->IsInlineable()) return false;
  }
  return true;
}


bool UnaryOperation::IsInlineable() const {
  return expression()->IsInlineable();
}


bool BinaryOperation::IsInlineable() const {
  return left()->IsInlineable() && right()->IsInlineable();
}


bool CompareOperation::IsInlineable() const {
  return left()->IsInlineable() && right()->IsInlineable();
}


bool CompareToNull::IsInlineable() const {
  return expression()->IsInlineable();
}


bool CountOperation::IsInlineable() const {
  return expression()->IsInlineable();
}


// ----------------------------------------------------------------------------
// Recording of type feedback

void Property::RecordTypeFeedback(TypeFeedbackOracle* oracle) {
  // Record type feedback from the oracle in the AST.
  is_monomorphic_ = oracle->LoadIsMonomorphicNormal(this);
  receiver_types_.Clear();
  if (key()->IsPropertyName()) {
    if (oracle->LoadIsBuiltin(this, Builtins::kLoadIC_ArrayLength)) {
      is_array_length_ = true;
    } else if (oracle->LoadIsBuiltin(this, Builtins::kLoadIC_StringLength)) {
      is_string_length_ = true;
    } else if (oracle->LoadIsBuiltin(this,
                                     Builtins::kLoadIC_FunctionPrototype)) {
      is_function_prototype_ = true;
    } else {
      Literal* lit_key = key()->AsLiteral();
      ASSERT(lit_key != NULL && lit_key->handle()->IsString());
      Handle<String> name = Handle<String>::cast(lit_key->handle());
      oracle->LoadReceiverTypes(this, name, &receiver_types_);
    }
  } else if (oracle->LoadIsBuiltin(this, Builtins::kKeyedLoadIC_String)) {
    is_string_access_ = true;
  } else if (is_monomorphic_) {
    receiver_types_.Add(oracle->LoadMonomorphicReceiverType(this));
  } else if (oracle->LoadIsMegamorphicWithTypeInfo(this)) {
    receiver_types_.Reserve(kMaxKeyedPolymorphism);
    oracle->CollectKeyedReceiverTypes(this->id(), &receiver_types_);
  }
}


void Assignment::RecordTypeFeedback(TypeFeedbackOracle* oracle) {
  Property* prop = target()->AsProperty();
  ASSERT(prop != NULL);
  is_monomorphic_ = oracle->StoreIsMonomorphicNormal(this);
  receiver_types_.Clear();
  if (prop->key()->IsPropertyName()) {
    Literal* lit_key = prop->key()->AsLiteral();
    ASSERT(lit_key != NULL && lit_key->handle()->IsString());
    Handle<String> name = Handle<String>::cast(lit_key->handle());
    oracle->StoreReceiverTypes(this, name, &receiver_types_);
  } else if (is_monomorphic_) {
    // Record receiver type for monomorphic keyed stores.
    receiver_types_.Add(oracle->StoreMonomorphicReceiverType(this));
  } else if (oracle->StoreIsMegamorphicWithTypeInfo(this)) {
    receiver_types_.Reserve(kMaxKeyedPolymorphism);
    oracle->CollectKeyedReceiverTypes(this->id(), &receiver_types_);
  }
}


void CountOperation::RecordTypeFeedback(TypeFeedbackOracle* oracle) {
  is_monomorphic_ = oracle->StoreIsMonomorphicNormal(this);
  receiver_types_.Clear();
  if (is_monomorphic_) {
    // Record receiver type for monomorphic keyed stores.
    receiver_types_.Add(oracle->StoreMonomorphicReceiverType(this));
  } else if (oracle->StoreIsMegamorphicWithTypeInfo(this)) {
    receiver_types_.Reserve(kMaxKeyedPolymorphism);
    oracle->CollectKeyedReceiverTypes(this->id(), &receiver_types_);
  }
}


void CaseClause::RecordTypeFeedback(TypeFeedbackOracle* oracle) {
  TypeInfo info = oracle->SwitchType(this);
  if (info.IsSmi()) {
    compare_type_ = SMI_ONLY;
  } else if (info.IsNonPrimitive()) {
    compare_type_ = OBJECT_ONLY;
  } else {
    ASSERT(compare_type_ == NONE);
  }
}


static bool CanCallWithoutIC(Handle<JSFunction> target, int arity) {
  SharedFunctionInfo* info = target->shared();
  // If the number of formal parameters of the target function does
  // not match the number of arguments we're passing, we don't want to
  // deal with it. Otherwise, we can call it directly.
  return !target->NeedsArgumentsAdaption() ||
      info->formal_parameter_count() == arity;
}


bool Call::ComputeTarget(Handle<Map> type, Handle<String> name) {
  if (check_type_ == RECEIVER_MAP_CHECK) {
    // For primitive checks the holder is set up to point to the
    // corresponding prototype object, i.e. one step of the algorithm
    // below has been already performed.
    // For non-primitive checks we clear it to allow computing targets
    // for polymorphic calls.
    holder_ = Handle<JSObject>::null();
  }
  while (true) {
    LookupResult lookup;
    type->LookupInDescriptors(NULL, *name, &lookup);
    // If the function wasn't found directly in the map, we start
    // looking upwards through the prototype chain.
    if (!lookup.IsFound() && type->prototype()->IsJSObject()) {
      holder_ = Handle<JSObject>(JSObject::cast(type->prototype()));
      type = Handle<Map>(holder()->map());
    } else if (lookup.IsProperty() && lookup.type() == CONSTANT_FUNCTION) {
      target_ = Handle<JSFunction>(lookup.GetConstantFunctionFromMap(*type));
      return CanCallWithoutIC(target_, arguments()->length());
    } else {
      return false;
    }
  }
}


bool Call::ComputeGlobalTarget(Handle<GlobalObject> global,
                               LookupResult* lookup) {
  target_ = Handle<JSFunction>::null();
  cell_ = Handle<JSGlobalPropertyCell>::null();
  ASSERT(lookup->IsProperty() &&
         lookup->type() == NORMAL &&
         lookup->holder() == *global);
  cell_ = Handle<JSGlobalPropertyCell>(global->GetPropertyCell(lookup));
  if (cell_->value()->IsJSFunction()) {
    Handle<JSFunction> candidate(JSFunction::cast(cell_->value()));
    // If the function is in new space we assume it's more likely to
    // change and thus prefer the general IC code.
    if (!HEAP->InNewSpace(*candidate) &&
        CanCallWithoutIC(candidate, arguments()->length())) {
      target_ = candidate;
      return true;
    }
  }
  return false;
}


void Call::RecordTypeFeedback(TypeFeedbackOracle* oracle,
                              CallKind call_kind) {
  Property* property = expression()->AsProperty();
  ASSERT(property != NULL);
  // Specialize for the receiver types seen at runtime.
  Literal* key = property->key()->AsLiteral();
  ASSERT(key != NULL && key->handle()->IsString());
  Handle<String> name = Handle<String>::cast(key->handle());
  receiver_types_.Clear();
  oracle->CallReceiverTypes(this, name, call_kind, &receiver_types_);
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) {
    int length = receiver_types_.length();
    for (int i = 0; i < length; i++) {
      Handle<Map> map = receiver_types_.at(i);
      ASSERT(!map.is_null() && *map != NULL);
    }
  }
#endif
  is_monomorphic_ = oracle->CallIsMonomorphic(this);
  check_type_ = oracle->GetCallCheckType(this);
  if (is_monomorphic_) {
    Handle<Map> map;
    if (receiver_types_.length() > 0) {
      ASSERT(check_type_ == RECEIVER_MAP_CHECK);
      map = receiver_types_.at(0);
    } else {
      ASSERT(check_type_ != RECEIVER_MAP_CHECK);
      holder_ = Handle<JSObject>(
          oracle->GetPrototypeForPrimitiveCheck(check_type_));
      map = Handle<Map>(holder_->map());
    }
    is_monomorphic_ = ComputeTarget(map, name);
  }
}


void CompareOperation::RecordTypeFeedback(TypeFeedbackOracle* oracle) {
  TypeInfo info = oracle->CompareType(this);
  if (info.IsSmi()) {
    compare_type_ = SMI_ONLY;
  } else if (info.IsNonPrimitive()) {
    compare_type_ = OBJECT_ONLY;
  } else {
    ASSERT(compare_type_ == NONE);
  }
}


// ----------------------------------------------------------------------------
// Implementation of AstVisitor

bool AstVisitor::CheckStackOverflow() {
  if (stack_overflow_) return true;
  StackLimitCheck check(isolate_);
  if (!check.HasOverflowed()) return false;
  return (stack_overflow_ = true);
}


void AstVisitor::VisitDeclarations(ZoneList<Declaration*>* declarations) {
  for (int i = 0; i < declarations->length(); i++) {
    Visit(declarations->at(i));
  }
}


void AstVisitor::VisitStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    Visit(statements->at(i));
  }
}


void AstVisitor::VisitExpressions(ZoneList<Expression*>* expressions) {
  for (int i = 0; i < expressions->length(); i++) {
    // The variable statement visiting code may pass NULL expressions
    // to this code. Maybe this should be handled by introducing an
    // undefined expression or literal?  Revisit this code if this
    // changes
    Expression* expression = expressions->at(i);
    if (expression != NULL) Visit(expression);
  }
}


// ----------------------------------------------------------------------------
// Regular expressions

#define MAKE_ACCEPT(Name)                                            \
  void* RegExp##Name::Accept(RegExpVisitor* visitor, void* data) {   \
    return visitor->Visit##Name(this, data);                         \
  }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_ACCEPT)
#undef MAKE_ACCEPT

#define MAKE_TYPE_CASE(Name)                                         \
  RegExp##Name* RegExpTree::As##Name() {                             \
    return NULL;                                                     \
  }                                                                  \
  bool RegExpTree::Is##Name() { return false; }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_TYPE_CASE)
#undef MAKE_TYPE_CASE

#define MAKE_TYPE_CASE(Name)                                        \
  RegExp##Name* RegExp##Name::As##Name() {                          \
    return this;                                                    \
  }                                                                 \
  bool RegExp##Name::Is##Name() { return true; }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_TYPE_CASE)
#undef MAKE_TYPE_CASE

RegExpEmpty RegExpEmpty::kInstance;


static Interval ListCaptureRegisters(ZoneList<RegExpTree*>* children) {
  Interval result = Interval::Empty();
  for (int i = 0; i < children->length(); i++)
    result = result.Union(children->at(i)->CaptureRegisters());
  return result;
}


Interval RegExpAlternative::CaptureRegisters() {
  return ListCaptureRegisters(nodes());
}


Interval RegExpDisjunction::CaptureRegisters() {
  return ListCaptureRegisters(alternatives());
}


Interval RegExpLookahead::CaptureRegisters() {
  return body()->CaptureRegisters();
}


Interval RegExpCapture::CaptureRegisters() {
  Interval self(StartRegister(index()), EndRegister(index()));
  return self.Union(body()->CaptureRegisters());
}


Interval RegExpQuantifier::CaptureRegisters() {
  return body()->CaptureRegisters();
}


bool RegExpAssertion::IsAnchoredAtStart() {
  return type() == RegExpAssertion::START_OF_INPUT;
}


bool RegExpAssertion::IsAnchoredAtEnd() {
  return type() == RegExpAssertion::END_OF_INPUT;
}


bool RegExpAlternative::IsAnchoredAtStart() {
  ZoneList<RegExpTree*>* nodes = this->nodes();
  for (int i = 0; i < nodes->length(); i++) {
    RegExpTree* node = nodes->at(i);
    if (node->IsAnchoredAtStart()) { return true; }
    if (node->max_match() > 0) { return false; }
  }
  return false;
}


bool RegExpAlternative::IsAnchoredAtEnd() {
  ZoneList<RegExpTree*>* nodes = this->nodes();
  for (int i = nodes->length() - 1; i >= 0; i--) {
    RegExpTree* node = nodes->at(i);
    if (node->IsAnchoredAtEnd()) { return true; }
    if (node->max_match() > 0) { return false; }
  }
  return false;
}


bool RegExpDisjunction::IsAnchoredAtStart() {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    if (!alternatives->at(i)->IsAnchoredAtStart())
      return false;
  }
  return true;
}


bool RegExpDisjunction::IsAnchoredAtEnd() {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    if (!alternatives->at(i)->IsAnchoredAtEnd())
      return false;
  }
  return true;
}


bool RegExpLookahead::IsAnchoredAtStart() {
  return is_positive() && body()->IsAnchoredAtStart();
}


bool RegExpCapture::IsAnchoredAtStart() {
  return body()->IsAnchoredAtStart();
}


bool RegExpCapture::IsAnchoredAtEnd() {
  return body()->IsAnchoredAtEnd();
}


// Convert regular expression trees to a simple sexp representation.
// This representation should be different from the input grammar
// in as many cases as possible, to make it more difficult for incorrect
// parses to look as correct ones which is likely if the input and
// output formats are alike.
class RegExpUnparser: public RegExpVisitor {
 public:
  RegExpUnparser();
  void VisitCharacterRange(CharacterRange that);
  SmartArrayPointer<const char> ToString() { return stream_.ToCString(); }
#define MAKE_CASE(Name) virtual void* Visit##Name(RegExp##Name*, void* data);
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_CASE)
#undef MAKE_CASE
 private:
  StringStream* stream() { return &stream_; }
  HeapStringAllocator alloc_;
  StringStream stream_;
};


RegExpUnparser::RegExpUnparser() : stream_(&alloc_) {
}


void* RegExpUnparser::VisitDisjunction(RegExpDisjunction* that, void* data) {
  stream()->Add("(|");
  for (int i = 0; i <  that->alternatives()->length(); i++) {
    stream()->Add(" ");
    that->alternatives()->at(i)->Accept(this, data);
  }
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitAlternative(RegExpAlternative* that, void* data) {
  stream()->Add("(:");
  for (int i = 0; i <  that->nodes()->length(); i++) {
    stream()->Add(" ");
    that->nodes()->at(i)->Accept(this, data);
  }
  stream()->Add(")");
  return NULL;
}


void RegExpUnparser::VisitCharacterRange(CharacterRange that) {
  stream()->Add("%k", that.from());
  if (!that.IsSingleton()) {
    stream()->Add("-%k", that.to());
  }
}



void* RegExpUnparser::VisitCharacterClass(RegExpCharacterClass* that,
                                          void* data) {
  if (that->is_negated())
    stream()->Add("^");
  stream()->Add("[");
  for (int i = 0; i < that->ranges()->length(); i++) {
    if (i > 0) stream()->Add(" ");
    VisitCharacterRange(that->ranges()->at(i));
  }
  stream()->Add("]");
  return NULL;
}


void* RegExpUnparser::VisitAssertion(RegExpAssertion* that, void* data) {
  switch (that->type()) {
    case RegExpAssertion::START_OF_INPUT:
      stream()->Add("@^i");
      break;
    case RegExpAssertion::END_OF_INPUT:
      stream()->Add("@$i");
      break;
    case RegExpAssertion::START_OF_LINE:
      stream()->Add("@^l");
      break;
    case RegExpAssertion::END_OF_LINE:
      stream()->Add("@$l");
       break;
    case RegExpAssertion::BOUNDARY:
      stream()->Add("@b");
      break;
    case RegExpAssertion::NON_BOUNDARY:
      stream()->Add("@B");
      break;
  }
  return NULL;
}


void* RegExpUnparser::VisitAtom(RegExpAtom* that, void* data) {
  stream()->Add("'");
  Vector<const uc16> chardata = that->data();
  for (int i = 0; i < chardata.length(); i++) {
    stream()->Add("%k", chardata[i]);
  }
  stream()->Add("'");
  return NULL;
}


void* RegExpUnparser::VisitText(RegExpText* that, void* data) {
  if (that->elements()->length() == 1) {
    that->elements()->at(0).data.u_atom->Accept(this, data);
  } else {
    stream()->Add("(!");
    for (int i = 0; i < that->elements()->length(); i++) {
      stream()->Add(" ");
      that->elements()->at(i).data.u_atom->Accept(this, data);
    }
    stream()->Add(")");
  }
  return NULL;
}


void* RegExpUnparser::VisitQuantifier(RegExpQuantifier* that, void* data) {
  stream()->Add("(# %i ", that->min());
  if (that->max() == RegExpTree::kInfinity) {
    stream()->Add("- ");
  } else {
    stream()->Add("%i ", that->max());
  }
  stream()->Add(that->is_greedy() ? "g " : that->is_possessive() ? "p " : "n ");
  that->body()->Accept(this, data);
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitCapture(RegExpCapture* that, void* data) {
  stream()->Add("(^ ");
  that->body()->Accept(this, data);
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitLookahead(RegExpLookahead* that, void* data) {
  stream()->Add("(-> ");
  stream()->Add(that->is_positive() ? "+ " : "- ");
  that->body()->Accept(this, data);
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitBackReference(RegExpBackReference* that,
                                         void* data) {
  stream()->Add("(<- %i)", that->index());
  return NULL;
}


void* RegExpUnparser::VisitEmpty(RegExpEmpty* that, void* data) {
  stream()->Put('%');
  return NULL;
}


SmartArrayPointer<const char> RegExpTree::ToString() {
  RegExpUnparser unparser;
  Accept(&unparser, NULL);
  return unparser.ToString();
}


RegExpDisjunction::RegExpDisjunction(ZoneList<RegExpTree*>* alternatives)
    : alternatives_(alternatives) {
  ASSERT(alternatives->length() > 1);
  RegExpTree* first_alternative = alternatives->at(0);
  min_match_ = first_alternative->min_match();
  max_match_ = first_alternative->max_match();
  for (int i = 1; i < alternatives->length(); i++) {
    RegExpTree* alternative = alternatives->at(i);
    min_match_ = Min(min_match_, alternative->min_match());
    max_match_ = Max(max_match_, alternative->max_match());
  }
}


RegExpAlternative::RegExpAlternative(ZoneList<RegExpTree*>* nodes)
    : nodes_(nodes) {
  ASSERT(nodes->length() > 1);
  min_match_ = 0;
  max_match_ = 0;
  for (int i = 0; i < nodes->length(); i++) {
    RegExpTree* node = nodes->at(i);
    min_match_ += node->min_match();
    int node_max_match = node->max_match();
    if (kInfinity - max_match_ < node_max_match) {
      max_match_ = kInfinity;
    } else {
      max_match_ += node->max_match();
    }
  }
}


CaseClause::CaseClause(Isolate* isolate,
                       Expression* label,
                       ZoneList<Statement*>* statements,
                       int pos)
    : label_(label),
      statements_(statements),
      position_(pos),
      compare_type_(NONE),
      compare_id_(AstNode::GetNextId(isolate)),
      entry_id_(AstNode::GetNextId(isolate)) {
}

} }  // namespace v8::internal
