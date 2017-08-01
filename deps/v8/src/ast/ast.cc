// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"

#include <cmath>  // For isfinite.

#include "src/ast/compile-time-value.h"
#include "src/ast/prettyprinter.h"
#include "src/ast/scopes.h"
#include "src/base/hashmap.h"
#include "src/builtins/builtins-constructor.h"
#include "src/builtins/builtins.h"
#include "src/code-stubs.h"
#include "src/contexts.h"
#include "src/conversions.h"
#include "src/double.h"
#include "src/elements.h"
#include "src/objects-inl.h"
#include "src/objects/literal-objects.h"
#include "src/objects/map.h"
#include "src/property-details.h"
#include "src/property.h"
#include "src/string-stream.h"
#include "src/type-info.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Implementation of other node functionality.

#ifdef DEBUG

static const char* NameForNativeContextIntrinsicIndex(uint32_t idx) {
  switch (idx) {
#define NATIVE_CONTEXT_FIELDS_IDX(NAME, Type, name) \
  case Context::NAME:                               \
    return #name;

    NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELDS_IDX)
#undef NATIVE_CONTEXT_FIELDS_IDX

    default:
      break;
  }

  return "UnknownIntrinsicIndex";
}

void AstNode::Print() { Print(Isolate::Current()); }

void AstNode::Print(Isolate* isolate) {
  AllowHandleDereference allow_deref;
  AstPrinter::PrintOut(isolate, this);
}


#endif  // DEBUG

#define RETURN_NODE(Node) \
  case k##Node:           \
    return static_cast<Node*>(this);

IterationStatement* AstNode::AsIterationStatement() {
  switch (node_type()) {
    ITERATION_NODE_LIST(RETURN_NODE);
    default:
      return nullptr;
  }
}

BreakableStatement* AstNode::AsBreakableStatement() {
  switch (node_type()) {
    BREAKABLE_NODE_LIST(RETURN_NODE);
    ITERATION_NODE_LIST(RETURN_NODE);
    default:
      return nullptr;
  }
}

MaterializedLiteral* AstNode::AsMaterializedLiteral() {
  switch (node_type()) {
    LITERAL_NODE_LIST(RETURN_NODE);
    default:
      return nullptr;
  }
}

#undef RETURN_NODE

bool Expression::IsSmiLiteral() const {
  return IsLiteral() && AsLiteral()->raw_value()->IsSmi();
}

bool Expression::IsNumberLiteral() const {
  return IsLiteral() && AsLiteral()->raw_value()->IsNumber();
}

bool Expression::IsStringLiteral() const {
  return IsLiteral() && AsLiteral()->raw_value()->IsString();
}

bool Expression::IsPropertyName() const {
  return IsLiteral() && AsLiteral()->IsPropertyName();
}

bool Expression::IsNullLiteral() const {
  if (!IsLiteral()) return false;
  return AsLiteral()->raw_value()->IsNull();
}

bool Expression::IsUndefinedLiteral() const {
  if (IsLiteral() && AsLiteral()->raw_value()->IsUndefined()) return true;

  const VariableProxy* var_proxy = AsVariableProxy();
  if (var_proxy == nullptr) return false;
  Variable* var = var_proxy->var();
  // The global identifier "undefined" is immutable. Everything
  // else could be reassigned.
  return var != NULL && var->IsUnallocated() &&
         var_proxy->raw_name()->IsOneByteEqualTo("undefined");
}

bool Expression::ToBooleanIsTrue() const {
  return IsLiteral() && AsLiteral()->ToBooleanIsTrue();
}

bool Expression::ToBooleanIsFalse() const {
  return IsLiteral() && AsLiteral()->ToBooleanIsFalse();
}

bool Expression::IsValidReferenceExpression() const {
  // We don't want expressions wrapped inside RewritableExpression to be
  // considered as valid reference expressions, as they will be rewritten
  // to something (most probably involving a do expression).
  if (IsRewritableExpression()) return false;
  return IsProperty() ||
         (IsVariableProxy() && AsVariableProxy()->IsValidReferenceExpression());
}

bool Expression::IsValidReferenceExpressionOrThis() const {
  return IsValidReferenceExpression() ||
         (IsVariableProxy() && AsVariableProxy()->is_this());
}

bool Expression::IsAnonymousFunctionDefinition() const {
  return (IsFunctionLiteral() &&
          AsFunctionLiteral()->IsAnonymousFunctionDefinition()) ||
         (IsClassLiteral() &&
          AsClassLiteral()->IsAnonymousFunctionDefinition());
}

void Expression::MarkTail() {
  if (IsConditional()) {
    AsConditional()->MarkTail();
  } else if (IsCall()) {
    AsCall()->MarkTail();
  } else if (IsBinaryOperation()) {
    AsBinaryOperation()->MarkTail();
  }
}

bool Statement::IsJump() const {
  switch (node_type()) {
#define JUMP_NODE_LIST(V) \
  V(Block)                \
  V(ExpressionStatement)  \
  V(ContinueStatement)    \
  V(BreakStatement)       \
  V(ReturnStatement)      \
  V(IfStatement)
#define GENERATE_CASE(Node) \
  case k##Node:             \
    return static_cast<const Node*>(this)->IsJump();
    JUMP_NODE_LIST(GENERATE_CASE)
#undef GENERATE_CASE
#undef JUMP_NODE_LIST
    default:
      return false;
  }
}

VariableProxy::VariableProxy(Variable* var, int start_position)
    : Expression(start_position, kVariableProxy),
      raw_name_(var->raw_name()),
      next_unresolved_(nullptr) {
  bit_field_ |= IsThisField::encode(var->is_this()) |
                IsAssignedField::encode(false) |
                IsResolvedField::encode(false) |
                HoleCheckModeField::encode(HoleCheckMode::kElided);
  BindTo(var);
}

VariableProxy::VariableProxy(const AstRawString* name,
                             VariableKind variable_kind, int start_position)
    : Expression(start_position, kVariableProxy),
      raw_name_(name),
      next_unresolved_(nullptr) {
  bit_field_ |= IsThisField::encode(variable_kind == THIS_VARIABLE) |
                IsAssignedField::encode(false) |
                IsResolvedField::encode(false) |
                HoleCheckModeField::encode(HoleCheckMode::kElided);
}

VariableProxy::VariableProxy(const VariableProxy* copy_from)
    : Expression(copy_from->position(), kVariableProxy),
      next_unresolved_(nullptr) {
  bit_field_ = copy_from->bit_field_;
  DCHECK(!copy_from->is_resolved());
  raw_name_ = copy_from->raw_name_;
}

void VariableProxy::BindTo(Variable* var) {
  DCHECK((is_this() && var->is_this()) || raw_name() == var->raw_name());
  set_var(var);
  set_is_resolved();
  var->set_is_used();
  if (is_assigned()) var->set_maybe_assigned();
}

void VariableProxy::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                        TypeofMode typeof_mode,
                                        FeedbackSlotCache* cache) {
  if (UsesVariableFeedbackSlot()) {
    // VariableProxies that point to the same Variable within a function can
    // make their loads from the same IC slot.
    if (var()->IsUnallocated() || var()->mode() == DYNAMIC_GLOBAL) {
      FeedbackSlot slot = cache->Get(typeof_mode, var());
      if (!slot.IsInvalid()) {
        variable_feedback_slot_ = slot;
        return;
      }
      variable_feedback_slot_ = spec->AddLoadGlobalICSlot(typeof_mode);
      cache->Put(typeof_mode, var(), variable_feedback_slot_);
    } else {
      variable_feedback_slot_ = spec->AddLoadICSlot();
    }
  }
}

static void AssignVectorSlots(Expression* expr, FeedbackVectorSpec* spec,
                              LanguageMode language_mode,
                              FeedbackSlot* out_slot) {
  Property* property = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);
  // TODO(ishell): consider using ICSlotCache for variables here.
  if (assign_type == VARIABLE &&
      expr->AsVariableProxy()->var()->IsUnallocated()) {
    *out_slot = spec->AddStoreGlobalICSlot(language_mode);

  } else if (assign_type == NAMED_PROPERTY) {
    *out_slot = spec->AddStoreICSlot(language_mode);

  } else if (assign_type == KEYED_PROPERTY) {
    *out_slot = spec->AddKeyedStoreICSlot(language_mode);
  }
}

void ForInStatement::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                         LanguageMode language_mode,
                                         FeedbackSlotCache* cache) {
  AssignVectorSlots(each(), spec, language_mode, &each_slot_);
  for_in_feedback_slot_ = spec->AddGeneralSlot();
}

Assignment::Assignment(Token::Value op, Expression* target, Expression* value,
                       int pos)
    : Expression(pos, kAssignment),
      target_(target),
      value_(value),
      binary_operation_(NULL) {
  bit_field_ |= IsUninitializedField::encode(false) |
                KeyTypeField::encode(ELEMENT) |
                StoreModeField::encode(STANDARD_STORE) | TokenField::encode(op);
}

void Assignment::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                     LanguageMode language_mode,
                                     FeedbackSlotCache* cache) {
  AssignVectorSlots(target(), spec, language_mode, &slot_);
}

void CountOperation::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                         LanguageMode language_mode,
                                         FeedbackSlotCache* cache) {
  AssignVectorSlots(expression(), spec, language_mode, &slot_);
  // Assign a slot to collect feedback about binary operations. Used only in
  // ignition. Fullcodegen uses AstId to record type feedback.
  binary_operation_slot_ = spec->AddInterpreterBinaryOpICSlot();
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

bool FunctionLiteral::ShouldEagerCompile() const {
  return scope()->ShouldEagerCompile();
}

void FunctionLiteral::SetShouldEagerCompile() {
  scope()->set_should_eager_compile();
}

bool FunctionLiteral::AllowsLazyCompilation() {
  return scope()->AllowsLazyCompilation();
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

FunctionKind FunctionLiteral::kind() const { return scope()->function_kind(); }

bool FunctionLiteral::NeedsHomeObject(Expression* expr) {
  if (expr == nullptr || !expr->IsFunctionLiteral()) return false;
  DCHECK_NOT_NULL(expr->AsFunctionLiteral()->scope());
  return expr->AsFunctionLiteral()->scope()->NeedsHomeObject();
}

void FunctionLiteral::ReplaceBodyAndScope(FunctionLiteral* other) {
  DCHECK_NULL(body_);
  DCHECK_NOT_NULL(scope_);
  DCHECK_NOT_NULL(other->scope());

  Scope* outer_scope = scope_->outer_scope();

  body_ = other->body();
  scope_ = other->scope();
  scope_->ReplaceOuterScope(outer_scope);
#ifdef DEBUG
  scope_->set_replaced_from_parse_task(true);
#endif

  function_length_ = other->function_length_;
}

ObjectLiteralProperty::ObjectLiteralProperty(Expression* key, Expression* value,
                                             Kind kind, bool is_computed_name)
    : LiteralProperty(key, value, is_computed_name),
      kind_(kind),
      emit_store_(true) {}

ObjectLiteralProperty::ObjectLiteralProperty(AstValueFactory* ast_value_factory,
                                             Expression* key, Expression* value,
                                             bool is_computed_name)
    : LiteralProperty(key, value, is_computed_name), emit_store_(true) {
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

FeedbackSlot LiteralProperty::GetStoreDataPropertySlot() const {
  int offset = FunctionLiteral::NeedsHomeObject(value_) ? 1 : 0;
  return GetSlot(offset);
}

void LiteralProperty::SetStoreDataPropertySlot(FeedbackSlot slot) {
  int offset = FunctionLiteral::NeedsHomeObject(value_) ? 1 : 0;
  return SetSlot(slot, offset);
}

bool LiteralProperty::NeedsSetFunctionName() const {
  return is_computed_name_ &&
         (value_->IsAnonymousFunctionDefinition() ||
          (value_->IsFunctionLiteral() &&
           IsConciseMethod(value_->AsFunctionLiteral()->kind())));
}

ClassLiteralProperty::ClassLiteralProperty(Expression* key, Expression* value,
                                           Kind kind, bool is_static,
                                           bool is_computed_name)
    : LiteralProperty(key, value, is_computed_name),
      kind_(kind),
      is_static_(is_static) {}

void ClassLiteral::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                       LanguageMode language_mode,
                                       FeedbackSlotCache* cache) {
  // This logic that computes the number of slots needed for vector store
  // ICs must mirror BytecodeGenerator::VisitClassLiteral.
  if (FunctionLiteral::NeedsHomeObject(constructor())) {
    home_object_slot_ = spec->AddStoreICSlot(language_mode);
  }

  if (NeedsProxySlot()) {
    proxy_slot_ = spec->AddStoreICSlot(language_mode);
  }

  for (int i = 0; i < properties()->length(); i++) {
    ClassLiteral::Property* property = properties()->at(i);
    Expression* value = property->value();
    if (FunctionLiteral::NeedsHomeObject(value)) {
      property->SetSlot(spec->AddStoreICSlot(language_mode));
    }
    property->SetStoreDataPropertySlot(
        spec->AddStoreDataPropertyInLiteralICSlot());
  }
}

bool ObjectLiteral::Property::IsCompileTimeValue() const {
  return kind_ == CONSTANT ||
      (kind_ == MATERIALIZED_LITERAL &&
       CompileTimeValue::IsCompileTimeValue(value_));
}


void ObjectLiteral::Property::set_emit_store(bool emit_store) {
  emit_store_ = emit_store;
}

bool ObjectLiteral::Property::emit_store() const { return emit_store_; }

void ObjectLiteral::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                        LanguageMode language_mode,
                                        FeedbackSlotCache* cache) {
  MaterializedLiteral::AssignFeedbackSlots(spec, language_mode, cache);

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
      case ObjectLiteral::Property::SPREAD:
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
      // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->IsStringLiteral()) {
          if (property->emit_store()) {
            property->SetSlot(spec->AddStoreOwnICSlot());
            if (FunctionLiteral::NeedsHomeObject(value)) {
              property->SetSlot(spec->AddStoreICSlot(language_mode), 1);
            }
          }
          break;
        }
        if (property->emit_store() && FunctionLiteral::NeedsHomeObject(value)) {
          property->SetSlot(spec->AddStoreICSlot(language_mode));
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        break;
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store() && FunctionLiteral::NeedsHomeObject(value)) {
          property->SetSlot(spec->AddStoreICSlot(language_mode));
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store() && FunctionLiteral::NeedsHomeObject(value)) {
          property->SetSlot(spec->AddStoreICSlot(language_mode));
        }
        break;
    }
  }

  for (; property_index < properties()->length(); property_index++) {
    ObjectLiteral::Property* property = properties()->at(property_index);

    Expression* value = property->value();
    if (!property->IsPrototype()) {
      if (FunctionLiteral::NeedsHomeObject(value)) {
        property->SetSlot(spec->AddStoreICSlot(language_mode));
      }
    }
    property->SetStoreDataPropertySlot(
        spec->AddStoreDataPropertyInLiteralICSlot());
  }
}


void ObjectLiteral::CalculateEmitStore(Zone* zone) {
  const auto GETTER = ObjectLiteral::Property::GETTER;
  const auto SETTER = ObjectLiteral::Property::SETTER;

  ZoneAllocationPolicy allocator(zone);

  CustomMatcherZoneHashMap table(
      Literal::Match, ZoneHashMap::kDefaultHashMapCapacity, allocator);
  for (int i = properties()->length() - 1; i >= 0; i--) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (property->is_computed_name()) continue;
    if (property->IsPrototype()) continue;
    Literal* literal = property->key()->AsLiteral();
    DCHECK(!literal->IsNullLiteral());

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

void ObjectLiteral::InitFlagsForPendingNullPrototype(int i) {
  // We still check for __proto__:null after computed property names.
  for (; i < properties()->length(); i++) {
    if (properties()->at(i)->IsNullPrototype()) {
      set_has_null_protoype(true);
      break;
    }
  }
}

void ObjectLiteral::InitDepthAndFlags() {
  if (is_initialized()) return;
  bool is_simple = true;
  bool has_seen_prototype = false;
  int depth_acc = 1;
  uint32_t nof_properties = 0;
  uint32_t elements = 0;
  uint32_t max_element_index = 0;
  for (int i = 0; i < properties()->length(); i++) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (property->IsPrototype()) {
      has_seen_prototype = true;
      // __proto__:null has no side-effects and is set directly on the
      // boilerplate.
      if (property->IsNullPrototype()) {
        set_has_null_protoype(true);
        continue;
      }
      DCHECK(!has_null_prototype());
      is_simple = false;
      continue;
    }
    if (nof_properties == boilerplate_properties_) {
      DCHECK(property->is_computed_name());
      is_simple = false;
      if (!has_seen_prototype) InitFlagsForPendingNullPrototype(i);
      break;
    }
    DCHECK(!property->is_computed_name());

    MaterializedLiteral* m_literal = property->value()->AsMaterializedLiteral();
    if (m_literal != NULL) {
      m_literal->InitDepthAndFlags();
      if (m_literal->depth() >= depth_acc) depth_acc = m_literal->depth() + 1;
    }

    const AstValue* key = property->key()->AsLiteral()->raw_value();
    Expression* value = property->value();

    bool is_compile_time_value = CompileTimeValue::IsCompileTimeValue(value);

    // Ensure objects that may, at any point in time, contain fields with double
    // representation are always treated as nested objects. This is true for
    // computed fields, and smi and double literals.
    // TODO(verwaest): Remove once we can store them inline.
    if (FLAG_track_double_fields &&
        (value->IsNumberLiteral() || !is_compile_time_value)) {
      set_may_store_doubles(true);
    }

    is_simple = is_simple && is_compile_time_value;

    // Keep track of the number of elements in the object literal and
    // the largest element index.  If the largest element index is
    // much larger than the number of elements, creating an object
    // literal with fast elements will be a waste of space.
    uint32_t element_index = 0;
    if (key->IsString() && key->AsString()->AsArrayIndex(&element_index)) {
      max_element_index = Max(element_index, max_element_index);
      elements++;
    } else if (key->ToUint32(&element_index) && element_index != kMaxUInt32) {
      max_element_index = Max(element_index, max_element_index);
      elements++;
    }

    nof_properties++;
  }

  set_fast_elements((max_element_index <= 32) ||
                    ((2 * elements) >= max_element_index));
  set_has_elements(elements > 0);
  set_is_simple(is_simple);
  set_depth(depth_acc);
}

void ObjectLiteral::BuildConstantProperties(Isolate* isolate) {
  if (!constant_properties_.is_null()) return;

  int index_keys = 0;
  bool has_seen_proto = false;
  for (int i = 0; i < properties()->length(); i++) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (property->IsPrototype()) {
      has_seen_proto = true;
      continue;
    }
    if (property->is_computed_name()) {
      continue;
    }

    Handle<Object> key = property->key()->AsLiteral()->value();

    uint32_t element_index = 0;
    if (key->ToArrayIndex(&element_index) ||
        (key->IsString() && String::cast(*key)->AsArrayIndex(&element_index))) {
      index_keys++;
    }
  }

  Handle<BoilerplateDescription> constant_properties =
      isolate->factory()->NewBoilerplateDescription(boilerplate_properties_,
                                                    properties()->length(),
                                                    index_keys, has_seen_proto);

  int position = 0;
  for (int i = 0; i < properties()->length(); i++) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (property->IsPrototype()) continue;

    if (static_cast<uint32_t>(position) == boilerplate_properties_ * 2) {
      DCHECK(property->is_computed_name());
      break;
    }
    DCHECK(!property->is_computed_name());

    MaterializedLiteral* m_literal = property->value()->AsMaterializedLiteral();
    if (m_literal != NULL) {
      m_literal->BuildConstants(isolate);
    }

    // Add CONSTANT and COMPUTED properties to boilerplate. Use undefined
    // value for COMPUTED properties, the real value is filled in at
    // runtime. The enumeration order is maintained.
    Handle<Object> key = property->key()->AsLiteral()->value();
    Handle<Object> value = GetBoilerplateValue(property->value(), isolate);

    uint32_t element_index = 0;
    if (key->IsString() && String::cast(*key)->AsArrayIndex(&element_index)) {
      key = isolate->factory()->NewNumberFromUint(element_index);
    } else if (key->IsNumber() && !key->ToArrayIndex(&element_index)) {
      key = isolate->factory()->NumberToString(key);
    }

    // Add name, value pair to the fixed array.
    constant_properties->set(position++, *key);
    constant_properties->set(position++, *value);
  }

  constant_properties_ = constant_properties;
}

bool ObjectLiteral::IsFastCloningSupported() const {
  // The FastCloneShallowObject builtin doesn't copy elements, and object
  // literals don't support copy-on-write (COW) elements for now.
  // TODO(mvstanton): make object literals support COW elements.
  return fast_elements() && has_shallow_properties() &&
         properties_count() <=
             ConstructorBuiltins::kMaximumClonedShallowObjectProperties;
}

ElementsKind ArrayLiteral::constant_elements_kind() const {
  return static_cast<ElementsKind>(constant_elements()->elements_kind());
}

void ArrayLiteral::InitDepthAndFlags() {
  DCHECK_LT(first_spread_index_, 0);

  if (is_initialized()) return;

  int constants_length = values()->length();

  // Fill in the literals.
  bool is_simple = true;
  int depth_acc = 1;
  int array_index = 0;
  for (; array_index < constants_length; array_index++) {
    Expression* element = values()->at(array_index);
    DCHECK(!element->IsSpread());
    MaterializedLiteral* m_literal = element->AsMaterializedLiteral();
    if (m_literal != NULL) {
      m_literal->InitDepthAndFlags();
      if (m_literal->depth() + 1 > depth_acc) {
        depth_acc = m_literal->depth() + 1;
      }
    }

    if (!CompileTimeValue::IsCompileTimeValue(element)) {
      is_simple = false;
    }
  }

  set_is_simple(is_simple);
  set_depth(depth_acc);
}

void ArrayLiteral::BuildConstantElements(Isolate* isolate) {
  DCHECK_LT(first_spread_index_, 0);

  if (!constant_elements_.is_null()) return;

  int constants_length = values()->length();
  ElementsKind kind = FIRST_FAST_ELEMENTS_KIND;
  Handle<FixedArray> fixed_array =
      isolate->factory()->NewFixedArrayWithHoles(constants_length);

  // Fill in the literals.
  bool is_holey = false;
  int array_index = 0;
  for (; array_index < constants_length; array_index++) {
    Expression* element = values()->at(array_index);
    DCHECK(!element->IsSpread());
    MaterializedLiteral* m_literal = element->AsMaterializedLiteral();
    if (m_literal != NULL) {
      m_literal->BuildConstants(isolate);
    }

    // New handle scope here, needs to be after BuildContants().
    HandleScope scope(isolate);
    Handle<Object> boilerplate_value = GetBoilerplateValue(element, isolate);
    if (boilerplate_value->IsTheHole(isolate)) {
      is_holey = true;
      continue;
    }

    if (boilerplate_value->IsUninitialized(isolate)) {
      boilerplate_value = handle(Smi::kZero, isolate);
    }

    kind = GetMoreGeneralElementsKind(kind,
                                      boilerplate_value->OptimalElementsKind());
    fixed_array->set(array_index, *boilerplate_value);
  }

  if (is_holey) kind = GetHoleyElementsKind(kind);

  // Simple and shallow arrays can be lazily copied, we transform the
  // elements array to a copy-on-write array.
  if (is_simple() && depth() == 1 && array_index > 0 &&
      IsFastSmiOrObjectElementsKind(kind)) {
    fixed_array->set_map(isolate->heap()->fixed_cow_array_map());
  }

  Handle<FixedArrayBase> elements = fixed_array;
  if (IsFastDoubleElementsKind(kind)) {
    ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
    elements = isolate->factory()->NewFixedDoubleArray(constants_length);
    // We are copying from non-fast-double to fast-double.
    ElementsKind from_kind = TERMINAL_FAST_ELEMENTS_KIND;
    accessor->CopyElements(fixed_array, from_kind, elements, constants_length);
  }

  // Remember both the literal's constant values as well as the ElementsKind.
  Handle<ConstantElementsPair> literals =
      isolate->factory()->NewConstantElementsPair(kind, elements);

  constant_elements_ = literals;
}

bool ArrayLiteral::IsFastCloningSupported() const {
  return depth() <= 1 &&
         values()->length() <=
             ConstructorBuiltins::kMaximumClonedShallowArrayElements;
}

void ArrayLiteral::RewindSpreads() {
  values_->Rewind(first_spread_index_);
  first_spread_index_ = -1;
}

void ArrayLiteral::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                       LanguageMode language_mode,
                                       FeedbackSlotCache* cache) {
  MaterializedLiteral::AssignFeedbackSlots(spec, language_mode, cache);

  // This logic that computes the number of slots needed for vector store
  // ics must mirror FullCodeGenerator::VisitArrayLiteral.
  for (int array_index = 0; array_index < values()->length(); array_index++) {
    Expression* subexpr = values()->at(array_index);
    DCHECK(!subexpr->IsSpread());
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    // We'll reuse the same literal slot for all of the non-constant
    // subexpressions that use a keyed store IC.
    literal_slot_ = spec->AddKeyedStoreICSlot(language_mode);
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

void MaterializedLiteral::InitDepthAndFlags() {
  if (IsArrayLiteral()) {
    return AsArrayLiteral()->InitDepthAndFlags();
  }
  if (IsObjectLiteral()) {
    return AsObjectLiteral()->InitDepthAndFlags();
  }
  DCHECK(IsRegExpLiteral());
  DCHECK_LE(1, depth());  // Depth should be initialized.
}

void MaterializedLiteral::BuildConstants(Isolate* isolate) {
  if (IsArrayLiteral()) {
    return AsArrayLiteral()->BuildConstantElements(isolate);
  }
  if (IsObjectLiteral()) {
    return AsObjectLiteral()->BuildConstantProperties(isolate);
  }
  DCHECK(IsRegExpLiteral());
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

void BinaryOperation::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                          LanguageMode language_mode,
                                          FeedbackSlotCache* cache) {
  // Feedback vector slot is only used by interpreter for binary operations.
  // Full-codegen uses AstId to record type feedback.
  switch (op()) {
    // Comma, logical_or and logical_and do not collect type feedback.
    case Token::COMMA:
    case Token::AND:
    case Token::OR:
      return;
    default:
      feedback_slot_ = spec->AddInterpreterBinaryOpICSlot();
      return;
  }
}

static bool IsCommutativeOperationWithSmiLiteral(Token::Value op) {
  // Add is not commutative due to potential for string addition.
  return op == Token::MUL || op == Token::BIT_AND || op == Token::BIT_OR ||
         op == Token::BIT_XOR;
}

// Check for the pattern: x + 1.
static bool MatchSmiLiteralOperation(Expression* left, Expression* right,
                                     Expression** expr, Smi** literal) {
  if (right->IsSmiLiteral()) {
    *expr = left;
    *literal = right->AsLiteral()->AsSmiLiteral();
    return true;
  }
  return false;
}

bool BinaryOperation::IsSmiLiteralOperation(Expression** subexpr,
                                            Smi** literal) {
  return MatchSmiLiteralOperation(left_, right_, subexpr, literal) ||
         (IsCommutativeOperationWithSmiLiteral(op()) &&
          MatchSmiLiteralOperation(right_, left_, subexpr, literal));
}

static bool IsTypeof(Expression* expr) {
  UnaryOperation* maybe_unary = expr->AsUnaryOperation();
  return maybe_unary != NULL && maybe_unary->op() == Token::TYPEOF;
}

void CompareOperation::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                           LanguageMode language_mode,
                                           FeedbackSlotCache* cache_) {
  // Feedback vector slot is only used by interpreter for binary operations.
  // Full-codegen uses AstId to record type feedback.
  switch (op()) {
    // instanceof and in do not collect type feedback.
    case Token::INSTANCEOF:
    case Token::IN:
      return;
    default:
      feedback_slot_ = spec->AddInterpreterCompareICSlot();
  }
}

// Check for the pattern: typeof <expression> equals <string literal>.
static bool MatchLiteralCompareTypeof(Expression* left, Token::Value op,
                                      Expression* right, Expression** expr,
                                      Literal** literal) {
  if (IsTypeof(left) && right->IsStringLiteral() && Token::IsEqualityOp(op)) {
    *expr = left->AsUnaryOperation()->expression();
    *literal = right->AsLiteral();
    return true;
  }
  return false;
}

bool CompareOperation::IsLiteralCompareTypeof(Expression** expr,
                                              Literal** literal) {
  return MatchLiteralCompareTypeof(left_, op(), right_, expr, literal) ||
         MatchLiteralCompareTypeof(right_, op(), left_, expr, literal);
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
                                         Expression** expr) {
  if (IsVoidOfLiteral(left) && Token::IsEqualityOp(op)) {
    *expr = right;
    return true;
  }
  if (left->IsUndefinedLiteral() && Token::IsEqualityOp(op)) {
    *expr = right;
    return true;
  }
  return false;
}

bool CompareOperation::IsLiteralCompareUndefined(Expression** expr) {
  return MatchLiteralCompareUndefined(left_, op(), right_, expr) ||
         MatchLiteralCompareUndefined(right_, op(), left_, expr);
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
  return MatchLiteralCompareNull(left_, op(), right_, expr) ||
         MatchLiteralCompareNull(right_, op(), left_, expr);
}


// ----------------------------------------------------------------------------
// Recording of type feedback

// TODO(rossberg): all RecordTypeFeedback functions should disappear
// once we use the common type field in the AST consistently.

void Expression::RecordToBooleanTypeFeedback(TypeFeedbackOracle* oracle) {
  if (IsUnaryOperation()) {
    AsUnaryOperation()->RecordToBooleanTypeFeedback(oracle);
  } else if (IsBinaryOperation()) {
    AsBinaryOperation()->RecordToBooleanTypeFeedback(oracle);
  } else {
    set_to_boolean_types(oracle->ToBooleanTypes(test_id()));
  }
}

void SmallMapList::AddMapIfMissing(Handle<Map> map, Zone* zone) {
  if (!Map::TryUpdate(map).ToHandle(&map)) return;
  for (int i = 0; i < length(); ++i) {
    if (at(i).is_identical_to(map)) return;
  }
  Add(map, zone);
}

void SmallMapList::FilterForPossibleTransitions(Map* root_map) {
  for (int i = list_.length() - 1; i >= 0; i--) {
    if (at(i)->FindRootMap() != root_map) {
      list_.RemoveElement(list_.at(i));
    }
  }
}

Handle<Map> SmallMapList::at(int i) const { return Handle<Map>(list_.at(i)); }

SmallMapList* Expression::GetReceiverTypes() {
  switch (node_type()) {
#define NODE_LIST(V)    \
  PROPERTY_NODE_LIST(V) \
  V(Call)
#define GENERATE_CASE(Node) \
  case k##Node:             \
    return static_cast<Node*>(this)->GetReceiverTypes();
    NODE_LIST(GENERATE_CASE)
#undef NODE_LIST
#undef GENERATE_CASE
    default:
      UNREACHABLE();
      return nullptr;
  }
}

KeyedAccessStoreMode Expression::GetStoreMode() const {
  switch (node_type()) {
#define GENERATE_CASE(Node) \
  case k##Node:             \
    return static_cast<const Node*>(this)->GetStoreMode();
    PROPERTY_NODE_LIST(GENERATE_CASE)
#undef GENERATE_CASE
    default:
      UNREACHABLE();
      return STANDARD_STORE;
  }
}

IcCheckType Expression::GetKeyType() const {
  switch (node_type()) {
#define GENERATE_CASE(Node) \
  case k##Node:             \
    return static_cast<const Node*>(this)->GetKeyType();
    PROPERTY_NODE_LIST(GENERATE_CASE)
#undef GENERATE_CASE
    default:
      UNREACHABLE();
      return PROPERTY;
  }
}

bool Expression::IsMonomorphic() const {
  switch (node_type()) {
#define GENERATE_CASE(Node) \
  case k##Node:             \
    return static_cast<const Node*>(this)->IsMonomorphic();
    PROPERTY_NODE_LIST(GENERATE_CASE)
    CALL_NODE_LIST(GENERATE_CASE)
#undef GENERATE_CASE
    default:
      UNREACHABLE();
      return false;
  }
}

void Call::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                               LanguageMode language_mode,
                               FeedbackSlotCache* cache) {
  ic_slot_ = spec->AddCallICSlot();
}

Call::CallType Call::GetCallType() const {
  VariableProxy* proxy = expression()->AsVariableProxy();
  if (proxy != NULL) {
    if (proxy->var()->IsUnallocated()) {
      return GLOBAL_CALL;
    } else if (proxy->var()->IsLookupSlot()) {
      // Calls going through 'with' always use DYNAMIC rather than DYNAMIC_LOCAL
      // or DYNAMIC_GLOBAL.
      return proxy->var()->mode() == DYNAMIC ? WITH_CALL : OTHER_CALL;
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

CaseClause::CaseClause(Expression* label, ZoneList<Statement*>* statements,
                       int pos)
    : Expression(pos, kCaseClause),
      label_(label),
      statements_(statements),
      compare_type_(AstType::None()) {}

void CaseClause::AssignFeedbackSlots(FeedbackVectorSpec* spec,
                                     LanguageMode language_mode,
                                     FeedbackSlotCache* cache) {
  feedback_slot_ = spec->AddInterpreterCompareICSlot();
}

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

const char* CallRuntime::debug_name() {
#ifdef DEBUG
  return is_jsruntime() ? NameForNativeContextIntrinsicIndex(context_index_)
                        : function_->name;
#else
  return is_jsruntime() ? "(context function)" : function_->name;
#endif  // DEBUG
}

}  // namespace internal
}  // namespace v8
