// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast.h"

#include <cmath>  // For isfinite.
#include "src/builtins.h"
#include "src/code-stubs.h"
#include "src/contexts.h"
#include "src/conversions.h"
#include "src/hashmap.h"
#include "src/parser.h"
#include "src/property.h"
#include "src/property-details.h"
#include "src/scopes.h"
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
  return var != NULL && var->location() == Variable::UNALLOCATED &&
         var_proxy->raw_name()->IsOneByteEqualTo("undefined");
}


VariableProxy::VariableProxy(Zone* zone, Variable* var, int start_position,
                             int end_position)
    : Expression(zone, start_position),
      bit_field_(IsThisField::encode(var->is_this()) |
                 IsAssignedField::encode(false) |
                 IsResolvedField::encode(false)),
      variable_feedback_slot_(FeedbackVectorICSlot::Invalid()),
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
      variable_feedback_slot_(FeedbackVectorICSlot::Invalid()),
      raw_name_(name),
      end_position_(end_position) {}


void VariableProxy::BindTo(Variable* var) {
  DCHECK((is_this() && var->is_this()) || raw_name() == var->raw_name());
  set_var(var);
  set_is_resolved();
  var->set_is_used();
}


void VariableProxy::SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                                           ICSlotCache* cache) {
  variable_feedback_slot_ = slot;
  if (var()->IsUnallocated()) {
    cache->Add(VariableICSlotPair(var(), slot));
  }
}


FeedbackVectorRequirements VariableProxy::ComputeFeedbackRequirements(
    Isolate* isolate, const ICSlotCache* cache) {
  if (UsesVariableFeedbackSlot()) {
    // VariableProxies that point to the same Variable within a function can
    // make their loads from the same IC slot.
    if (var()->IsUnallocated()) {
      for (int i = 0; i < cache->length(); i++) {
        VariableICSlotPair& pair = cache->at(i);
        if (pair.variable() == var()) {
          variable_feedback_slot_ = pair.slot();
          return FeedbackVectorRequirements(0, 0);
        }
      }
    }
    return FeedbackVectorRequirements(0, 1);
  }
  return FeedbackVectorRequirements(0, 0);
}


Assignment::Assignment(Zone* zone, Token::Value op, Expression* target,
                       Expression* value, int pos)
    : Expression(zone, pos),
      bit_field_(IsUninitializedField::encode(false) |
                 KeyTypeField::encode(ELEMENT) |
                 StoreModeField::encode(STANDARD_STORE) |
                 TokenField::encode(op)),
      target_(target),
      value_(value),
      binary_operation_(NULL) {}


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


bool FunctionLiteral::uses_super_property() const {
  DCHECK_NOT_NULL(scope());
  return scope()->uses_super_property() || scope()->inner_uses_super_property();
}


// Helper to find an existing shared function info in the baseline code for the
// given function literal. Used to canonicalize SharedFunctionInfo objects.
void FunctionLiteral::InitializeSharedInfo(
    Handle<Code> unoptimized_code) {
  for (RelocIterator it(*unoptimized_code); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    if (rinfo->rmode() != RelocInfo::EMBEDDED_OBJECT) continue;
    Object* obj = rinfo->target_object();
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
      if (shared->start_position() == start_position()) {
        shared_info_ = Handle<SharedFunctionInfo>(shared);
        break;
      }
    }
  }
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
    ZoneHashMap::Entry* entry = table.Lookup(literal, hash, true, allocator);
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
  set_is_simple(is_simple);
  set_depth(depth_acc);
}


void ArrayLiteral::BuildConstantElements(Isolate* isolate) {
  if (!constant_elements_.is_null()) return;

  // Allocate a fixed array to hold all the object literals.
  Handle<JSArray> array =
      isolate->factory()->NewJSArray(0, FAST_HOLEY_SMI_ELEMENTS);
  JSArray::Expand(array, values()->length());

  // Fill in the literals.
  bool is_simple = true;
  int depth_acc = 1;
  bool is_holey = false;
  for (int i = 0, n = values()->length(); i < n; i++) {
    Expression* element = values()->at(i);
    MaterializedLiteral* m_literal = element->AsMaterializedLiteral();
    if (m_literal != NULL) {
      m_literal->BuildConstants(isolate);
      if (m_literal->depth() + 1 > depth_acc) {
        depth_acc = m_literal->depth() + 1;
      }
    }
    Handle<Object> boilerplate_value = GetBoilerplateValue(element, isolate);
    if (boilerplate_value->IsTheHole()) {
      is_holey = true;
    } else if (boilerplate_value->IsUninitialized()) {
      is_simple = false;
      JSObject::SetOwnElement(
          array, i, handle(Smi::FromInt(0), isolate), SLOPPY).Assert();
    } else {
      JSObject::SetOwnElement(array, i, boilerplate_value, SLOPPY).Assert();
    }
  }

  Handle<FixedArrayBase> element_values(array->elements());

  // Simple and shallow arrays can be lazily copied, we transform the
  // elements array to a copy-on-write array.
  if (is_simple && depth_acc == 1 && values()->length() > 0 &&
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
  if (IsUsingCallFeedbackSlot(isolate) || call_type == POSSIBLY_EVAL_CALL) {
    return false;
  }
  return true;
}


bool Call::IsUsingCallFeedbackSlot(Isolate* isolate) const {
  // SuperConstructorCall uses a CallConstructStub, which wants
  // a Slot, not an IC slot.
  return GetCallType(isolate) == SUPER_CALL;
}


FeedbackVectorRequirements Call::ComputeFeedbackRequirements(
    Isolate* isolate, const ICSlotCache* cache) {
  int ic_slots = IsUsingCallFeedbackICSlot(isolate) ? 1 : 0;
  int slots = IsUsingCallFeedbackSlot(isolate) ? 1 : 0;
  // A Call uses either a slot or an IC slot.
  DCHECK((ic_slots & slots) == 0);
  return FeedbackVectorRequirements(slots, ic_slots);
}


Call::CallType Call::GetCallType(Isolate* isolate) const {
  VariableProxy* proxy = expression()->AsVariableProxy();
  if (proxy != NULL) {
    if (proxy->var()->is_possibly_eval(isolate)) {
      return POSSIBLY_EVAL_CALL;
    } else if (proxy->var()->IsUnallocated()) {
      return GLOBAL_CALL;
    } else if (proxy->var()->IsLookupSlot()) {
      return LOOKUP_SLOT_CALL;
    }
  }

  if (expression()->AsSuperReference() != NULL) return SUPER_CALL;

  Property* property = expression()->AsProperty();
  return property != NULL ? PROPERTY_CALL : OTHER_CALL;
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
  return assertion_type() == RegExpAssertion::START_OF_INPUT;
}


bool RegExpAssertion::IsAnchoredAtEnd() {
  return assertion_type() == RegExpAssertion::END_OF_INPUT;
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
class RegExpUnparser FINAL : public RegExpVisitor {
 public:
  RegExpUnparser(std::ostream& os, Zone* zone) : os_(os), zone_(zone) {}
  void VisitCharacterRange(CharacterRange that);
#define MAKE_CASE(Name) virtual void* Visit##Name(RegExp##Name*,          \
                                                  void* data) OVERRIDE;
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_CASE)
#undef MAKE_CASE
 private:
  std::ostream& os_;
  Zone* zone_;
};


void* RegExpUnparser::VisitDisjunction(RegExpDisjunction* that, void* data) {
  os_ << "(|";
  for (int i = 0; i <  that->alternatives()->length(); i++) {
    os_ << " ";
    that->alternatives()->at(i)->Accept(this, data);
  }
  os_ << ")";
  return NULL;
}


void* RegExpUnparser::VisitAlternative(RegExpAlternative* that, void* data) {
  os_ << "(:";
  for (int i = 0; i <  that->nodes()->length(); i++) {
    os_ << " ";
    that->nodes()->at(i)->Accept(this, data);
  }
  os_ << ")";
  return NULL;
}


void RegExpUnparser::VisitCharacterRange(CharacterRange that) {
  os_ << AsUC16(that.from());
  if (!that.IsSingleton()) {
    os_ << "-" << AsUC16(that.to());
  }
}



void* RegExpUnparser::VisitCharacterClass(RegExpCharacterClass* that,
                                          void* data) {
  if (that->is_negated()) os_ << "^";
  os_ << "[";
  for (int i = 0; i < that->ranges(zone_)->length(); i++) {
    if (i > 0) os_ << " ";
    VisitCharacterRange(that->ranges(zone_)->at(i));
  }
  os_ << "]";
  return NULL;
}


void* RegExpUnparser::VisitAssertion(RegExpAssertion* that, void* data) {
  switch (that->assertion_type()) {
    case RegExpAssertion::START_OF_INPUT:
      os_ << "@^i";
      break;
    case RegExpAssertion::END_OF_INPUT:
      os_ << "@$i";
      break;
    case RegExpAssertion::START_OF_LINE:
      os_ << "@^l";
      break;
    case RegExpAssertion::END_OF_LINE:
      os_ << "@$l";
       break;
    case RegExpAssertion::BOUNDARY:
      os_ << "@b";
      break;
    case RegExpAssertion::NON_BOUNDARY:
      os_ << "@B";
      break;
  }
  return NULL;
}


void* RegExpUnparser::VisitAtom(RegExpAtom* that, void* data) {
  os_ << "'";
  Vector<const uc16> chardata = that->data();
  for (int i = 0; i < chardata.length(); i++) {
    os_ << AsUC16(chardata[i]);
  }
  os_ << "'";
  return NULL;
}


void* RegExpUnparser::VisitText(RegExpText* that, void* data) {
  if (that->elements()->length() == 1) {
    that->elements()->at(0).tree()->Accept(this, data);
  } else {
    os_ << "(!";
    for (int i = 0; i < that->elements()->length(); i++) {
      os_ << " ";
      that->elements()->at(i).tree()->Accept(this, data);
    }
    os_ << ")";
  }
  return NULL;
}


void* RegExpUnparser::VisitQuantifier(RegExpQuantifier* that, void* data) {
  os_ << "(# " << that->min() << " ";
  if (that->max() == RegExpTree::kInfinity) {
    os_ << "- ";
  } else {
    os_ << that->max() << " ";
  }
  os_ << (that->is_greedy() ? "g " : that->is_possessive() ? "p " : "n ");
  that->body()->Accept(this, data);
  os_ << ")";
  return NULL;
}


void* RegExpUnparser::VisitCapture(RegExpCapture* that, void* data) {
  os_ << "(^ ";
  that->body()->Accept(this, data);
  os_ << ")";
  return NULL;
}


void* RegExpUnparser::VisitLookahead(RegExpLookahead* that, void* data) {
  os_ << "(-> " << (that->is_positive() ? "+ " : "- ");
  that->body()->Accept(this, data);
  os_ << ")";
  return NULL;
}


void* RegExpUnparser::VisitBackReference(RegExpBackReference* that,
                                         void* data) {
  os_ << "(<- " << that->index() << ")";
  return NULL;
}


void* RegExpUnparser::VisitEmpty(RegExpEmpty* that, void* data) {
  os_ << '%';
  return NULL;
}


std::ostream& RegExpTree::Print(std::ostream& os, Zone* zone) {  // NOLINT
  RegExpUnparser unparser(os, zone);
  Accept(&unparser, NULL);
  return os;
}


RegExpDisjunction::RegExpDisjunction(ZoneList<RegExpTree*>* alternatives)
    : alternatives_(alternatives) {
  DCHECK(alternatives->length() > 1);
  RegExpTree* first_alternative = alternatives->at(0);
  min_match_ = first_alternative->min_match();
  max_match_ = first_alternative->max_match();
  for (int i = 1; i < alternatives->length(); i++) {
    RegExpTree* alternative = alternatives->at(i);
    min_match_ = Min(min_match_, alternative->min_match());
    max_match_ = Max(max_match_, alternative->max_match());
  }
}


static int IncreaseBy(int previous, int increase) {
  if (RegExpTree::kInfinity - previous < increase) {
    return RegExpTree::kInfinity;
  } else {
    return previous + increase;
  }
}

RegExpAlternative::RegExpAlternative(ZoneList<RegExpTree*>* nodes)
    : nodes_(nodes) {
  DCHECK(nodes->length() > 1);
  min_match_ = 0;
  max_match_ = 0;
  for (int i = 0; i < nodes->length(); i++) {
    RegExpTree* node = nodes->at(i);
    int node_min_match = node->min_match();
    min_match_ = IncreaseBy(min_match_, node_min_match);
    int node_max_match = node->max_match();
    max_match_ = IncreaseBy(max_match_, node_max_match);
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
  return (x->IsString() && y->IsString() && *x->AsString() == *y->AsString()) ||
         (x->IsNumber() && y->IsNumber() && x->AsNumber() == y->AsNumber());
}


} }  // namespace v8::internal
