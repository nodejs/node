// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"

#include <cmath>  // For isfinite.
#include "src/ast/scopes.h"
#include "src/builtins.h"
#include "src/code-stubs.h"
#include "src/contexts.h"
#include "src/conversions.h"
#include "src/hashmap.h"
#include "src/parsing/parser.h"
#include "src/property.h"
#include "src/property-details.h"
#include "src/string-stream.h"
#include "src/type-info.h"

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


bool Expression::IsSmiLiteral() const {
  return IsLiteral() && AsLiteral()->value()->IsSmi();
}


bool Expression::IsStringLiteral() const {
  return IsLiteral() && AsLiteral()->value()->IsString();
}


bool Expression::IsNullLiteral() const {
  return IsLiteral() && AsLiteral()->value()->IsNull();
}


bool Expression::IsUndefinedLiteral(Isolate* isolate) const {
  const VariableProxy* var_proxy = AsVariableProxy();
  if (var_proxy == NULL) return false;
  Variable* var = var_proxy->var();
  // The global identifier "undefined" is immutable. Everything
  // else could be reassigned.
  return var != NULL && var->IsUnallocatedOrGlobalSlot() &&
         var_proxy->raw_name()->IsOneByteEqualTo("undefined");
}


bool Expression::IsValidReferenceExpressionOrThis() const {
  return IsValidReferenceExpression() ||
         (IsVariableProxy() && AsVariableProxy()->is_this());
}


VariableProxy::VariableProxy(Zone* zone, Variable* var, int start_position,
                             int end_position)
    : Expression(zone, start_position),
      bit_field_(IsThisField::encode(var->is_this()) |
                 IsAssignedField::encode(false) |
                 IsResolvedField::encode(false)),
      raw_name_(var->raw_name()),
      end_position_(end_position) {
  BindTo(var);
}


VariableProxy::VariableProxy(Zone* zone, const AstRawString* name,
                             Variable::Kind variable_kind, int start_position,
                             int end_position)
    : Expression(zone, start_position),
      bit_field_(IsThisField::encode(variable_kind == Variable::THIS) |
                 IsAssignedField::encode(false) |
                 IsResolvedField::encode(false)),
      raw_name_(name),
      end_position_(end_position) {}


void VariableProxy::BindTo(Variable* var) {
  DCHECK((is_this() && var->is_this()) || raw_name() == var->raw_name());
  set_var(var);
  set_is_resolved();
  var->set_is_used();
}


void VariableProxy::AssignFeedbackVectorSlots(Isolate* isolate,
                                              FeedbackVectorSpec* spec,
                                              FeedbackVectorSlotCache* cache) {
  if (UsesVariableFeedbackSlot()) {
    // VariableProxies that point to the same Variable within a function can
    // make their loads from the same IC slot.
    if (var()->IsUnallocated()) {
      ZoneHashMap::Entry* entry = cache->Get(var());
      if (entry != NULL) {
        variable_feedback_slot_ = FeedbackVectorSlot(
            static_cast<int>(reinterpret_cast<intptr_t>(entry->value)));
        return;
      }
    }
    variable_feedback_slot_ = spec->AddLoadICSlot();
    if (var()->IsUnallocated()) {
      cache->Put(var(), variable_feedback_slot_);
    }
  }
}


static void AssignVectorSlots(Expression* expr, FeedbackVectorSpec* spec,
                              FeedbackVectorSlot* out_slot) {
  Property* property = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);
  if ((assign_type == VARIABLE &&
       expr->AsVariableProxy()->var()->IsUnallocated()) ||
      assign_type == NAMED_PROPERTY || assign_type == KEYED_PROPERTY) {
    // TODO(ishell): consider using ICSlotCache for variables here.
    FeedbackVectorSlotKind kind = assign_type == KEYED_PROPERTY
                                      ? FeedbackVectorSlotKind::KEYED_STORE_IC
                                      : FeedbackVectorSlotKind::STORE_IC;
    *out_slot = spec->AddSlot(kind);
  }
}


void ForEachStatement::AssignFeedbackVectorSlots(
    Isolate* isolate, FeedbackVectorSpec* spec,
    FeedbackVectorSlotCache* cache) {
  // TODO(adamk): for-of statements do not make use of this feedback slot.
  // The each_slot_ should be specific to ForInStatement, and this work moved
  // there.
  if (IsForOfStatement()) return;
  AssignVectorSlots(each(), spec, &each_slot_);
}


Assignment::Assignment(Zone* zone, Token::Value op, Expression* target,
                       Expression* value, int pos)
    : Expression(zone, pos),
      bit_field_(
          IsUninitializedField::encode(false) | KeyTypeField::encode(ELEMENT) |
          StoreModeField::encode(STANDARD_STORE) | TokenField::encode(op)),
      target_(target),
      value_(value),
      binary_operation_(NULL) {}


void Assignment::AssignFeedbackVectorSlots(Isolate* isolate,
                                           FeedbackVectorSpec* spec,
                                           FeedbackVectorSlotCache* cache) {
  AssignVectorSlots(target(), spec, &slot_);
}


void CountOperation::AssignFeedbackVectorSlots(Isolate* isolate,
                                               FeedbackVectorSpec* spec,
                                               FeedbackVectorSlotCache* cache) {
  AssignVectorSlots(expression(), spec, &slot_);
}


Token::Value Assignment::binary_op() const {
  switch (op()) {
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


bool FunctionLiteral::AllowsLazyCompilationWithoutContext() {
  return scope()->AllowsLazyCompilationWithoutContext();
}


int FunctionLiteral::start_position() const {
  return scope()->start_position();
}


int FunctionLiteral::end_position() const {
  return scope()->end_position();
}


LanguageMode FunctionLiteral::language_mode() const {
  return scope()->language_mode();
}


bool FunctionLiteral::NeedsHomeObject(Expression* expr) {
  if (expr == nullptr || !expr->IsFunctionLiteral()) return false;
  DCHECK_NOT_NULL(expr->AsFunctionLiteral()->scope());
  return expr->AsFunctionLiteral()->scope()->NeedsHomeObject();
}


ObjectLiteralProperty::ObjectLiteralProperty(Expression* key, Expression* value,
                                             Kind kind, bool is_static,
                                             bool is_computed_name)
    : key_(key),
      value_(value),
      kind_(kind),
      emit_store_(true),
      is_static_(is_static),
      is_computed_name_(is_computed_name) {}


ObjectLiteralProperty::ObjectLiteralProperty(AstValueFactory* ast_value_factory,
                                             Expression* key, Expression* value,
                                             bool is_static,
                                             bool is_computed_name)
    : key_(key),
      value_(value),
      emit_store_(true),
      is_static_(is_static),
      is_computed_name_(is_computed_name) {
  if (!is_computed_name &&
      key->AsLiteral()->raw_value()->EqualsString(
          ast_value_factory->proto_string())) {
    kind_ = PROTOTYPE;
  } else if (value_->AsMaterializedLiteral() != NULL) {
    kind_ = MATERIALIZED_LITERAL;
  } else if (value_->IsLiteral()) {
    kind_ = CONSTANT;
  } else {
    kind_ = COMPUTED;
  }
}


void ClassLiteral::AssignFeedbackVectorSlots(Isolate* isolate,
                                             FeedbackVectorSpec* spec,
                                             FeedbackVectorSlotCache* cache) {
  // This logic that computes the number of slots needed for vector store
  // ICs must mirror FullCodeGenerator::VisitClassLiteral.
  if (NeedsProxySlot()) {
    slot_ = spec->AddStoreICSlot();
  }

  for (int i = 0; i < properties()->length(); i++) {
    ObjectLiteral::Property* property = properties()->at(i);
    Expression* value = property->value();
    if (FunctionLiteral::NeedsHomeObject(value)) {
      property->SetSlot(spec->AddStoreICSlot());
    }
  }
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


void ObjectLiteral::AssignFeedbackVectorSlots(Isolate* isolate,
                                              FeedbackVectorSpec* spec,
                                              FeedbackVectorSlotCache* cache) {
  // This logic that computes the number of slots needed for vector store
  // ics must mirror FullCodeGenerator::VisitObjectLiteral.
  int property_index = 0;
  for (; property_index < properties()->length(); property_index++) {
    ObjectLiteral::Property* property = properties()->at(property_index);
    if (property->is_computed_name()) break;
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
      // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            property->SetSlot(spec->AddStoreICSlot());
            if (FunctionLiteral::NeedsHomeObject(value)) {
              property->SetSlot(spec->AddStoreICSlot(), 1);
            }
          }
          break;
        }
        if (property->emit_store() && FunctionLiteral::NeedsHomeObject(value)) {
          property->SetSlot(spec->AddStoreICSlot());
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        break;
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store() && FunctionLiteral::NeedsHomeObject(value)) {
          property->SetSlot(spec->AddStoreICSlot());
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store() && FunctionLiteral::NeedsHomeObject(value)) {
          property->SetSlot(spec->AddStoreICSlot());
        }
        break;
    }
  }

  for (; property_index < properties()->length(); property_index++) {
    ObjectLiteral::Property* property = properties()->at(property_index);

    Expression* value = property->value();
    if (property->kind() != ObjectLiteral::Property::PROTOTYPE) {
      if (FunctionLiteral::NeedsHomeObject(value)) {
        property->SetSlot(spec->AddStoreICSlot());
      }
    }
  }
}


void ObjectLiteral::CalculateEmitStore(Zone* zone) {
  const auto GETTER = ObjectLiteral::Property::GETTER;
  const auto SETTER = ObjectLiteral::Property::SETTER;

  ZoneAllocationPolicy allocator(zone);

  ZoneHashMap table(Literal::Match, ZoneHashMap::kDefaultHashMapCapacity,
                    allocator);
  for (int i = properties()->length() - 1; i >= 0; i--) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (property->is_computed_name()) continue;
    if (property->kind() == ObjectLiteral::Property::PROTOTYPE) continue;
    Literal* literal = property->key()->AsLiteral();
    DCHECK(!literal->value()->IsNull());

    // If there is an existing entry do not emit a store unless the previous
    // entry was also an accessor.
    uint32_t hash = literal->Hash();
    ZoneHashMap::Entry* entry = table.LookupOrInsert(literal, hash, allocator);
    if (entry->value != NULL) {
      auto previous_kind =
          static_cast<ObjectLiteral::Property*>(entry->value)->kind();
      if (!((property->kind() == GETTER && previous_kind == SETTER) ||
            (property->kind() == SETTER && previous_kind == GETTER))) {
        property->set_emit_store(false);
      }
    }
    entry->value = property;
  }
}


bool ObjectLiteral::IsBoilerplateProperty(ObjectLiteral::Property* property) {
  return property != NULL &&
         property->kind() != ObjectLiteral::Property::PROTOTYPE;
}


void ObjectLiteral::BuildConstantProperties(Isolate* isolate) {
  if (!constant_properties_.is_null()) return;

  // Allocate a fixed array to hold all the constant properties.
  Handle<FixedArray> constant_properties = isolate->factory()->NewFixedArray(
      boilerplate_properties_ * 2, TENURED);

  int position = 0;
  // Accumulate the value in local variables and store it at the end.
  bool is_simple = true;
  int depth_acc = 1;
  uint32_t max_element_index = 0;
  uint32_t elements = 0;
  for (int i = 0; i < properties()->length(); i++) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (!IsBoilerplateProperty(property)) {
      is_simple = false;
      continue;
    }

    if (position == boilerplate_properties_ * 2) {
      DCHECK(property->is_computed_name());
      is_simple = false;
      break;
    }
    DCHECK(!property->is_computed_name());

    MaterializedLiteral* m_literal = property->value()->AsMaterializedLiteral();
    if (m_literal != NULL) {
      m_literal->BuildConstants(isolate);
      if (m_literal->depth() >= depth_acc) depth_acc = m_literal->depth() + 1;
    }

    // Add CONSTANT and COMPUTED properties to boilerplate. Use undefined
    // value for COMPUTED properties, the real value is filled in at
    // runtime. The enumeration order is maintained.
    Handle<Object> key = property->key()->AsLiteral()->value();
    Handle<Object> value = GetBoilerplateValue(property->value(), isolate);

    // Ensure objects that may, at any point in time, contain fields with double
    // representation are always treated as nested objects. This is true for
    // computed fields (value is undefined), and smi and double literals
    // (value->IsNumber()).
    // TODO(verwaest): Remove once we can store them inline.
    if (FLAG_track_double_fields &&
        (value->IsNumber() || value->IsUninitialized())) {
      may_store_doubles_ = true;
    }

    is_simple = is_simple && !value->IsUninitialized();

    // Keep track of the number of elements in the object literal and
    // the largest element index.  If the largest element index is
    // much larger than the number of elements, creating an object
    // literal with fast elements will be a waste of space.
    uint32_t element_index = 0;
    if (key->IsString()
        && Handle<String>::cast(key)->AsArrayIndex(&element_index)
        && element_index > max_element_index) {
      max_element_index = element_index;
      elements++;
    } else if (key->IsSmi()) {
      int key_value = Smi::cast(*key)->value();
      if (key_value > 0
          && static_cast<uint32_t>(key_value) > max_element_index) {
        max_element_index = key_value;
      }
      elements++;
    }

    // Add name, value pair to the fixed array.
    constant_properties->set(position++, *key);
    constant_properties->set(position++, *value);
  }

  constant_properties_ = constant_properties;
  fast_elements_ =
      (max_element_index <= 32) || ((2 * elements) >= max_element_index);
  has_elements_ = elements > 0;
  set_is_simple(is_simple);
  set_depth(depth_acc);
}


void ArrayLiteral::BuildConstantElements(Isolate* isolate) {
  if (!constant_elements_.is_null()) return;

  int constants_length =
      first_spread_index_ >= 0 ? first_spread_index_ : values()->length();

  // Allocate a fixed array to hold all the object literals.
  Handle<JSArray> array = isolate->factory()->NewJSArray(
      FAST_HOLEY_SMI_ELEMENTS, constants_length, constants_length,
      Strength::WEAK, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);

  // Fill in the literals.
  bool is_simple = (first_spread_index_ < 0);
  int depth_acc = 1;
  bool is_holey = false;
  int array_index = 0;
  for (; array_index < constants_length; array_index++) {
    Expression* element = values()->at(array_index);
    DCHECK(!element->IsSpread());
    MaterializedLiteral* m_literal = element->AsMaterializedLiteral();
    if (m_literal != NULL) {
      m_literal->BuildConstants(isolate);
      if (m_literal->depth() + 1 > depth_acc) {
        depth_acc = m_literal->depth() + 1;
      }
    }

    // New handle scope here, needs to be after BuildContants().
    HandleScope scope(isolate);
    Handle<Object> boilerplate_value = GetBoilerplateValue(element, isolate);
    if (boilerplate_value->IsTheHole()) {
      is_holey = true;
      continue;
    }

    if (boilerplate_value->IsUninitialized()) {
      boilerplate_value = handle(Smi::FromInt(0), isolate);
      is_simple = false;
    }

    JSObject::AddDataElement(array, array_index, boilerplate_value, NONE)
        .Assert();
  }

  JSObject::ValidateElements(array);
  Handle<FixedArrayBase> element_values(array->elements());

  // Simple and shallow arrays can be lazily copied, we transform the
  // elements array to a copy-on-write array.
  if (is_simple && depth_acc == 1 && array_index > 0 &&
      array->HasFastSmiOrObjectElements()) {
    element_values->set_map(isolate->heap()->fixed_cow_array_map());
  }

  // Remember both the literal's constant values as well as the ElementsKind
  // in a 2-element FixedArray.
  Handle<FixedArray> literals = isolate->factory()->NewFixedArray(2, TENURED);

  ElementsKind kind = array->GetElementsKind();
  kind = is_holey ? GetHoleyElementsKind(kind) : GetPackedElementsKind(kind);

  literals->set(0, Smi::FromInt(kind));
  literals->set(1, *element_values);

  constant_elements_ = literals;
  set_is_simple(is_simple);
  set_depth(depth_acc);
}


void ArrayLiteral::AssignFeedbackVectorSlots(Isolate* isolate,
                                             FeedbackVectorSpec* spec,
                                             FeedbackVectorSlotCache* cache) {
  // This logic that computes the number of slots needed for vector store
  // ics must mirror FullCodeGenerator::VisitArrayLiteral.
  int array_index = 0;
  for (; array_index < values()->length(); array_index++) {
    Expression* subexpr = values()->at(array_index);
    if (subexpr->IsSpread()) break;
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    // We'll reuse the same literal slot for all of the non-constant
    // subexpressions that use a keyed store IC.
    literal_slot_ = spec->AddKeyedStoreICSlot();
    return;
  }
}


Handle<Object> MaterializedLiteral::GetBoilerplateValue(Expression* expression,
                                                        Isolate* isolate) {
  if (expression->IsLiteral()) {
    return expression->AsLiteral()->value();
  }
  if (CompileTimeValue::IsCompileTimeValue(expression)) {
    return CompileTimeValue::GetValue(isolate, expression);
  }
  return isolate->factory()->uninitialized_value();
}


void MaterializedLiteral::BuildConstants(Isolate* isolate) {
  if (IsArrayLiteral()) {
    return AsArrayLiteral()->BuildConstantElements(isolate);
  }
  if (IsObjectLiteral()) {
    return AsObjectLiteral()->BuildConstantProperties(isolate);
  }
  DCHECK(IsRegExpLiteral());
  DCHECK(depth() >= 1);  // Depth should be initialized.
}


void UnaryOperation::RecordToBooleanTypeFeedback(TypeFeedbackOracle* oracle) {
  // TODO(olivf) If this Operation is used in a test context, then the
  // expression has a ToBoolean stub and we want to collect the type
  // information. However the GraphBuilder expects it to be on the instruction
  // corresponding to the TestContext, therefore we have to store it here and
  // not on the operand.
  set_to_boolean_types(oracle->ToBooleanTypes(expression()->test_id()));
}


void BinaryOperation::RecordToBooleanTypeFeedback(TypeFeedbackOracle* oracle) {
  // TODO(olivf) If this Operation is used in a test context, then the right
  // hand side has a ToBoolean stub and we want to collect the type information.
  // However the GraphBuilder expects it to be on the instruction corresponding
  // to the TestContext, therefore we have to store it here and not on the
  // right hand operand.
  set_to_boolean_types(oracle->ToBooleanTypes(right()->test_id()));
}


static bool IsTypeof(Expression* expr) {
  UnaryOperation* maybe_unary = expr->AsUnaryOperation();
  return maybe_unary != NULL && maybe_unary->op() == Token::TYPEOF;
}


// Check for the pattern: typeof <expression> equals <string literal>.
static bool MatchLiteralCompareTypeof(Expression* left,
                                      Token::Value op,
                                      Expression* right,
                                      Expression** expr,
                                      Handle<String>* check) {
  if (IsTypeof(left) && right->IsStringLiteral() && Token::IsEqualityOp(op)) {
    *expr = left->AsUnaryOperation()->expression();
    *check = Handle<String>::cast(right->AsLiteral()->value());
    return true;
  }
  return false;
}


bool CompareOperation::IsLiteralCompareTypeof(Expression** expr,
                                              Handle<String>* check) {
  return MatchLiteralCompareTypeof(left_, op_, right_, expr, check) ||
      MatchLiteralCompareTypeof(right_, op_, left_, expr, check);
}


static bool IsVoidOfLiteral(Expression* expr) {
  UnaryOperation* maybe_unary = expr->AsUnaryOperation();
  return maybe_unary != NULL &&
      maybe_unary->op() == Token::VOID &&
      maybe_unary->expression()->IsLiteral();
}


// Check for the pattern: void <literal> equals <expression> or
// undefined equals <expression>
static bool MatchLiteralCompareUndefined(Expression* left,
                                         Token::Value op,
                                         Expression* right,
                                         Expression** expr,
                                         Isolate* isolate) {
  if (IsVoidOfLiteral(left) && Token::IsEqualityOp(op)) {
    *expr = right;
    return true;
  }
  if (left->IsUndefinedLiteral(isolate) && Token::IsEqualityOp(op)) {
    *expr = right;
    return true;
  }
  return false;
}


bool CompareOperation::IsLiteralCompareUndefined(
    Expression** expr, Isolate* isolate) {
  return MatchLiteralCompareUndefined(left_, op_, right_, expr, isolate) ||
      MatchLiteralCompareUndefined(right_, op_, left_, expr, isolate);
}


// Check for the pattern: null equals <expression>
static bool MatchLiteralCompareNull(Expression* left,
                                    Token::Value op,
                                    Expression* right,
                                    Expression** expr) {
  if (left->IsNullLiteral() && Token::IsEqualityOp(op)) {
    *expr = right;
    return true;
  }
  return false;
}


bool CompareOperation::IsLiteralCompareNull(Expression** expr) {
  return MatchLiteralCompareNull(left_, op_, right_, expr) ||
      MatchLiteralCompareNull(right_, op_, left_, expr);
}


// ----------------------------------------------------------------------------
// Inlining support

bool Declaration::IsInlineable() const {
  return proxy()->var()->IsStackAllocated();
}

bool FunctionDeclaration::IsInlineable() const {
  return false;
}


// ----------------------------------------------------------------------------
// Recording of type feedback

// TODO(rossberg): all RecordTypeFeedback functions should disappear
// once we use the common type field in the AST consistently.

void Expression::RecordToBooleanTypeFeedback(TypeFeedbackOracle* oracle) {
  set_to_boolean_types(oracle->ToBooleanTypes(test_id()));
}


bool Call::IsUsingCallFeedbackICSlot(Isolate* isolate) const {
  CallType call_type = GetCallType(isolate);
  if (call_type == POSSIBLY_EVAL_CALL) {
    return false;
  }
  return true;
}


bool Call::IsUsingCallFeedbackSlot(Isolate* isolate) const {
  // SuperConstructorCall uses a CallConstructStub, which wants
  // a Slot, in addition to any IC slots requested elsewhere.
  return GetCallType(isolate) == SUPER_CALL;
}


void Call::AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                     FeedbackVectorSlotCache* cache) {
  if (IsUsingCallFeedbackICSlot(isolate)) {
    ic_slot_ = spec->AddCallICSlot();
  }
  if (IsUsingCallFeedbackSlot(isolate)) {
    stub_slot_ = spec->AddGeneralSlot();
  }
}


Call::CallType Call::GetCallType(Isolate* isolate) const {
  VariableProxy* proxy = expression()->AsVariableProxy();
  if (proxy != NULL) {
    if (proxy->var()->is_possibly_eval(isolate)) {
      return POSSIBLY_EVAL_CALL;
    } else if (proxy->var()->IsUnallocatedOrGlobalSlot()) {
      return GLOBAL_CALL;
    } else if (proxy->var()->IsLookupSlot()) {
      return LOOKUP_SLOT_CALL;
    }
  }

  if (expression()->IsSuperCallReference()) return SUPER_CALL;

  Property* property = expression()->AsProperty();
  if (property != nullptr) {
    bool is_super = property->IsSuperAccess();
    if (property->key()->IsPropertyName()) {
      return is_super ? NAMED_SUPER_PROPERTY_CALL : NAMED_PROPERTY_CALL;
    } else {
      return is_super ? KEYED_SUPER_PROPERTY_CALL : KEYED_PROPERTY_CALL;
    }
  }

  return OTHER_CALL;
}


// ----------------------------------------------------------------------------
// Implementation of AstVisitor

void AstVisitor::VisitDeclarations(ZoneList<Declaration*>* declarations) {
  for (int i = 0; i < declarations->length(); i++) {
    Visit(declarations->at(i));
  }
}


void AstVisitor::VisitStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    Statement* stmt = statements->at(i);
    Visit(stmt);
    if (stmt->IsJump()) break;
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


CaseClause::CaseClause(Zone* zone, Expression* label,
                       ZoneList<Statement*>* statements, int pos)
    : Expression(zone, pos),
      label_(label),
      statements_(statements),
      compare_type_(Type::None(zone)) {}


uint32_t Literal::Hash() {
  return raw_value()->IsString()
             ? raw_value()->AsString()->hash()
             : ComputeLongHash(double_to_uint64(raw_value()->AsNumber()));
}


// static
bool Literal::Match(void* literal1, void* literal2) {
  const AstValue* x = static_cast<Literal*>(literal1)->raw_value();
  const AstValue* y = static_cast<Literal*>(literal2)->raw_value();
  return (x->IsString() && y->IsString() && x->AsString() == y->AsString()) ||
         (x->IsNumber() && y->IsNumber() && x->AsNumber() == y->AsNumber());
}


}  // namespace internal
}  // namespace v8
