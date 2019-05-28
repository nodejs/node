// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"

#include <cmath>  // For isfinite.
#include <vector>

#include "src/ast/prettyprinter.h"
#include "src/ast/scopes.h"
#include "src/base/hashmap.h"
#include "src/builtins/builtins-constructor.h"
#include "src/builtins/builtins.h"
#include "src/contexts.h"
#include "src/conversions-inl.h"
#include "src/double.h"
#include "src/elements.h"
#include "src/objects-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/literal-objects.h"
#include "src/objects/map.h"
#include "src/property-details.h"
#include "src/property.h"
#include "src/string-stream.h"
#include "src/zone/zone-list-inl.h"

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

MaterializedLiteral* AstNode::AsMaterializedLiteral() {
  switch (node_type()) {
    LITERAL_NODE_LIST(RETURN_NODE);
    default:
      return nullptr;
  }
}

#undef RETURN_NODE

bool Expression::IsSmiLiteral() const {
  return IsLiteral() && AsLiteral()->type() == Literal::kSmi;
}

bool Expression::IsNumberLiteral() const {
  return IsLiteral() && AsLiteral()->IsNumber();
}

bool Expression::IsStringLiteral() const {
  return IsLiteral() && AsLiteral()->type() == Literal::kString;
}

bool Expression::IsPropertyName() const {
  return IsLiteral() && AsLiteral()->IsPropertyName();
}

bool Expression::IsNullLiteral() const {
  return IsLiteral() && AsLiteral()->type() == Literal::kNull;
}

bool Expression::IsTheHoleLiteral() const {
  return IsLiteral() && AsLiteral()->type() == Literal::kTheHole;
}

bool Expression::IsCompileTimeValue() {
  if (IsLiteral()) return true;
  MaterializedLiteral* literal = AsMaterializedLiteral();
  if (literal == nullptr) return false;
  return literal->IsSimple();
}

bool Expression::IsUndefinedLiteral() const {
  if (IsLiteral() && AsLiteral()->type() == Literal::kUndefined) return true;

  const VariableProxy* var_proxy = AsVariableProxy();
  if (var_proxy == nullptr) return false;
  Variable* var = var_proxy->var();
  // The global identifier "undefined" is immutable. Everything
  // else could be reassigned.
  return var != nullptr && var->IsUnallocated() &&
         var_proxy->raw_name()->IsOneByteEqualTo("undefined");
}

bool Expression::ToBooleanIsTrue() const {
  return IsLiteral() && AsLiteral()->ToBooleanIsTrue();
}

bool Expression::ToBooleanIsFalse() const {
  return IsLiteral() && AsLiteral()->ToBooleanIsFalse();
}

bool Expression::IsValidReferenceExpression() const {
  return IsProperty() ||
         (IsVariableProxy() && AsVariableProxy()->IsValidReferenceExpression());
}

bool Expression::IsAnonymousFunctionDefinition() const {
  return (IsFunctionLiteral() &&
          AsFunctionLiteral()->IsAnonymousFunctionDefinition()) ||
         (IsClassLiteral() &&
          AsClassLiteral()->IsAnonymousFunctionDefinition());
}

bool Expression::IsConciseMethodDefinition() const {
  return IsFunctionLiteral() && IsConciseMethod(AsFunctionLiteral()->kind());
}

bool Expression::IsAccessorFunctionDefinition() const {
  return IsFunctionLiteral() && IsAccessorFunction(AsFunctionLiteral()->kind());
}

VariableProxy::VariableProxy(Variable* var, int start_position)
    : Expression(start_position, kVariableProxy),
      raw_name_(var->raw_name()),
      next_unresolved_(nullptr) {
  DCHECK(!var->is_this());
  bit_field_ |= IsAssignedField::encode(false) |
                IsResolvedField::encode(false) |
                HoleCheckModeField::encode(HoleCheckMode::kElided);
  BindTo(var);
}

VariableProxy::VariableProxy(const VariableProxy* copy_from)
    : Expression(copy_from->position(), kVariableProxy),
      next_unresolved_(nullptr) {
  bit_field_ = copy_from->bit_field_;
  DCHECK(!copy_from->is_resolved());
  raw_name_ = copy_from->raw_name_;
}

void VariableProxy::BindTo(Variable* var) {
  DCHECK_EQ(raw_name(), var->raw_name());
  set_var(var);
  set_is_resolved();
  var->set_is_used();
  if (is_assigned()) var->set_maybe_assigned();
}

Assignment::Assignment(NodeType node_type, Token::Value op, Expression* target,
                       Expression* value, int pos)
    : Expression(pos, node_type), target_(target), value_(value) {
  bit_field_ |= TokenField::encode(op);
}

void FunctionLiteral::set_inferred_name(Handle<String> inferred_name) {
  DCHECK(!inferred_name.is_null());
  inferred_name_ = inferred_name;
  DCHECK(raw_inferred_name_ == nullptr || raw_inferred_name_->IsEmpty());
  raw_inferred_name_ = nullptr;
  scope()->set_has_inferred_function_name(true);
}

void FunctionLiteral::set_raw_inferred_name(
    const AstConsString* raw_inferred_name) {
  DCHECK_NOT_NULL(raw_inferred_name);
  raw_inferred_name_ = raw_inferred_name;
  DCHECK(inferred_name_.is_null());
  inferred_name_ = Handle<String>();
  scope()->set_has_inferred_function_name(true);
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

bool FunctionLiteral::SafeToSkipArgumentsAdaptor() const {
  // TODO(bmeurer,verwaest): The --fast_calls_with_arguments_mismatches
  // is mostly here for checking the real-world impact of the calling
  // convention. There's not really a point in turning off this flag
  // otherwise, so we should remove it at some point, when we're done
  // with the experiments (https://crbug.com/v8/8895).
  return FLAG_fast_calls_with_arguments_mismatches &&
         language_mode() == LanguageMode::kStrict &&
         scope()->arguments() == nullptr &&
         scope()->rest_parameter() == nullptr;
}

Handle<String> FunctionLiteral::name(Isolate* isolate) const {
  return raw_name_ ? raw_name_->string() : isolate->factory()->empty_string();
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

std::unique_ptr<char[]> FunctionLiteral::GetDebugName() const {
  const AstConsString* cons_string;
  if (raw_name_ != nullptr && !raw_name_->IsEmpty()) {
    cons_string = raw_name_;
  } else if (raw_inferred_name_ != nullptr && !raw_inferred_name_->IsEmpty()) {
    cons_string = raw_inferred_name_;
  } else if (!inferred_name_.is_null()) {
    AllowHandleDereference allow_deref;
    return inferred_name_->ToCString();
  } else {
    char* empty_str = new char[1];
    empty_str[0] = 0;
    return std::unique_ptr<char[]>(empty_str);
  }

  // TODO(rmcilroy): Deal with two-character strings.
  std::vector<char> result_vec;
  std::forward_list<const AstRawString*> strings = cons_string->ToRawStrings();
  for (const AstRawString* string : strings) {
    if (!string->is_one_byte()) break;
    for (int i = 0; i < string->length(); i++) {
      result_vec.push_back(string->raw_data()[i]);
    }
  }
  std::unique_ptr<char[]> result(new char[result_vec.size() + 1]);
  memcpy(result.get(), result_vec.data(), result_vec.size());
  result[result_vec.size()] = '\0';
  return result;
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
  if (!is_computed_name && key->AsLiteral()->IsString() &&
      key->AsLiteral()->AsRawString() == ast_value_factory->proto_string()) {
    kind_ = PROTOTYPE;
  } else if (value_->AsMaterializedLiteral() != nullptr) {
    kind_ = MATERIALIZED_LITERAL;
  } else if (value_->IsLiteral()) {
    kind_ = CONSTANT;
  } else {
    kind_ = COMPUTED;
  }
}

bool LiteralProperty::NeedsSetFunctionName() const {
  return is_computed_name() && (value_->IsAnonymousFunctionDefinition() ||
                                value_->IsConciseMethodDefinition() ||
                                value_->IsAccessorFunctionDefinition());
}

ClassLiteralProperty::ClassLiteralProperty(Expression* key, Expression* value,
                                           Kind kind, bool is_static,
                                           bool is_computed_name,
                                           bool is_private)
    : LiteralProperty(key, value, is_computed_name),
      kind_(kind),
      is_static_(is_static),
      is_private_(is_private),
      private_or_computed_name_var_(nullptr) {}

bool ObjectLiteral::Property::IsCompileTimeValue() const {
  return kind_ == CONSTANT ||
         (kind_ == MATERIALIZED_LITERAL && value_->IsCompileTimeValue());
}


void ObjectLiteral::Property::set_emit_store(bool emit_store) {
  emit_store_ = emit_store;
}

bool ObjectLiteral::Property::emit_store() const { return emit_store_; }

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

    uint32_t hash = literal->Hash();
    ZoneHashMap::Entry* entry = table.LookupOrInsert(literal, hash, allocator);
    if (entry->value == nullptr) {
      entry->value = property;
    } else {
      // We already have a later definition of this property, so we don't need
      // to emit a store for the current one.
      //
      // There are two subtleties here.
      //
      // (1) Emitting a store might actually be incorrect. For example, in {get
      // foo() {}, foo: 42}, the getter store would override the data property
      // (which, being a non-computed compile-time valued property, is already
      // part of the initial literal object.
      //
      // (2) If the later definition is an accessor (say, a getter), and the
      // current definition is a complementary accessor (here, a setter), then
      // we still must emit a store for the current definition.

      auto later_kind =
          static_cast<ObjectLiteral::Property*>(entry->value)->kind();
      bool complementary_accessors =
          (property->kind() == GETTER && later_kind == SETTER) ||
          (property->kind() == SETTER && later_kind == GETTER);
      if (!complementary_accessors) {
        property->set_emit_store(false);
        if (later_kind == GETTER || later_kind == SETTER) {
          entry->value = property;
        }
      }
    }
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

int ObjectLiteral::InitDepthAndFlags() {
  if (is_initialized()) return depth();
  bool is_simple = true;
  bool has_seen_prototype = false;
  bool needs_initial_allocation_site = false;
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

    MaterializedLiteral* literal = property->value()->AsMaterializedLiteral();
    if (literal != nullptr) {
      int subliteral_depth = literal->InitDepthAndFlags() + 1;
      if (subliteral_depth > depth_acc) depth_acc = subliteral_depth;
      needs_initial_allocation_site |= literal->NeedsInitialAllocationSite();
    }

    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();

    bool is_compile_time_value = value->IsCompileTimeValue();
    is_simple = is_simple && is_compile_time_value;

    // Keep track of the number of elements in the object literal and
    // the largest element index.  If the largest element index is
    // much larger than the number of elements, creating an object
    // literal with fast elements will be a waste of space.
    uint32_t element_index = 0;
    if (key->AsArrayIndex(&element_index)) {
      max_element_index = Max(element_index, max_element_index);
      elements++;
    } else {
      DCHECK(key->IsPropertyName());
    }

    nof_properties++;
  }

  set_depth(depth_acc);
  set_is_simple(is_simple);
  set_needs_initial_allocation_site(needs_initial_allocation_site);
  set_has_elements(elements > 0);
  set_fast_elements((max_element_index <= 32) ||
                    ((2 * elements) >= max_element_index));
  return depth_acc;
}

void ObjectLiteral::BuildBoilerplateDescription(Isolate* isolate) {
  if (!boilerplate_description_.is_null()) return;

  int index_keys = 0;
  bool has_seen_proto = false;
  for (int i = 0; i < properties()->length(); i++) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (property->IsPrototype()) {
      has_seen_proto = true;
      continue;
    }
    if (property->is_computed_name()) continue;

    Literal* key = property->key()->AsLiteral();
    if (!key->IsPropertyName()) index_keys++;
  }

  Handle<ObjectBoilerplateDescription> boilerplate_description =
      isolate->factory()->NewObjectBoilerplateDescription(
          boilerplate_properties_, properties()->length(), index_keys,
          has_seen_proto);

  int position = 0;
  for (int i = 0; i < properties()->length(); i++) {
    ObjectLiteral::Property* property = properties()->at(i);
    if (property->IsPrototype()) continue;

    if (static_cast<uint32_t>(position) == boilerplate_properties_) {
      DCHECK(property->is_computed_name());
      break;
    }
    DCHECK(!property->is_computed_name());

    MaterializedLiteral* m_literal = property->value()->AsMaterializedLiteral();
    if (m_literal != nullptr) {
      m_literal->BuildConstants(isolate);
    }

    // Add CONSTANT and COMPUTED properties to boilerplate. Use undefined
    // value for COMPUTED properties, the real value is filled in at
    // runtime. The enumeration order is maintained.
    Literal* key_literal = property->key()->AsLiteral();
    uint32_t element_index = 0;
    Handle<Object> key =
        key_literal->AsArrayIndex(&element_index)
            ? isolate->factory()->NewNumberFromUint(element_index)
            : Handle<Object>::cast(key_literal->AsRawPropertyName()->string());

    Handle<Object> value = GetBoilerplateValue(property->value(), isolate);

    // Add name, value pair to the fixed array.
    boilerplate_description->set_key_value(position++, *key, *value);
  }

  boilerplate_description->set_flags(EncodeLiteralType());

  boilerplate_description_ = boilerplate_description;
}

bool ObjectLiteral::IsFastCloningSupported() const {
  // The CreateShallowObjectLiteratal builtin doesn't copy elements, and object
  // literals don't support copy-on-write (COW) elements for now.
  // TODO(mvstanton): make object literals support COW elements.
  return fast_elements() && is_shallow() &&
         properties_count() <=
             ConstructorBuiltins::kMaximumClonedShallowObjectProperties;
}

int ArrayLiteral::InitDepthAndFlags() {
  if (is_initialized()) return depth();

  int constants_length =
      first_spread_index_ >= 0 ? first_spread_index_ : values()->length();

  // Fill in the literals.
  bool is_simple = first_spread_index_ < 0;
  int depth_acc = 1;
  int array_index = 0;
  for (; array_index < constants_length; array_index++) {
    Expression* element = values()->at(array_index);
    MaterializedLiteral* literal = element->AsMaterializedLiteral();
    if (literal != nullptr) {
      int subliteral_depth = literal->InitDepthAndFlags() + 1;
      if (subliteral_depth > depth_acc) depth_acc = subliteral_depth;
    }

    if (!element->IsCompileTimeValue()) {
      is_simple = false;
    }
  }

  set_depth(depth_acc);
  set_is_simple(is_simple);
  // Array literals always need an initial allocation site to properly track
  // elements transitions.
  set_needs_initial_allocation_site(true);
  return depth_acc;
}

void ArrayLiteral::BuildBoilerplateDescription(Isolate* isolate) {
  if (!boilerplate_description_.is_null()) return;

  int constants_length =
      first_spread_index_ >= 0 ? first_spread_index_ : values()->length();
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
    if (m_literal != nullptr) {
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
      IsSmiOrObjectElementsKind(kind)) {
    fixed_array->set_map(ReadOnlyRoots(isolate).fixed_cow_array_map());
  }

  Handle<FixedArrayBase> elements = fixed_array;
  if (IsDoubleElementsKind(kind)) {
    ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
    elements = isolate->factory()->NewFixedDoubleArray(constants_length);
    // We are copying from non-fast-double to fast-double.
    ElementsKind from_kind = TERMINAL_FAST_ELEMENTS_KIND;
    accessor->CopyElements(isolate, fixed_array, from_kind, elements,
                           constants_length);
  }

  boilerplate_description_ =
      isolate->factory()->NewArrayBoilerplateDescription(kind, elements);
}

bool ArrayLiteral::IsFastCloningSupported() const {
  return depth() <= 1 &&
         values_.length() <=
             ConstructorBuiltins::kMaximumClonedShallowArrayElements;
}

bool MaterializedLiteral::IsSimple() const {
  if (IsArrayLiteral()) return AsArrayLiteral()->is_simple();
  if (IsObjectLiteral()) return AsObjectLiteral()->is_simple();
  DCHECK(IsRegExpLiteral());
  return false;
}

Handle<Object> MaterializedLiteral::GetBoilerplateValue(Expression* expression,
                                                        Isolate* isolate) {
  if (expression->IsLiteral()) {
    return expression->AsLiteral()->BuildValue(isolate);
  }
  if (expression->IsCompileTimeValue()) {
    if (expression->IsObjectLiteral()) {
      ObjectLiteral* object_literal = expression->AsObjectLiteral();
      DCHECK(object_literal->is_simple());
      return object_literal->boilerplate_description();
    } else {
      DCHECK(expression->IsArrayLiteral());
      ArrayLiteral* array_literal = expression->AsArrayLiteral();
      DCHECK(array_literal->is_simple());
      return array_literal->boilerplate_description();
    }
  }
  return isolate->factory()->uninitialized_value();
}

int MaterializedLiteral::InitDepthAndFlags() {
  if (IsArrayLiteral()) return AsArrayLiteral()->InitDepthAndFlags();
  if (IsObjectLiteral()) return AsObjectLiteral()->InitDepthAndFlags();
  DCHECK(IsRegExpLiteral());
  return 1;
}

bool MaterializedLiteral::NeedsInitialAllocationSite() {
  if (IsArrayLiteral()) {
    return AsArrayLiteral()->needs_initial_allocation_site();
  }
  if (IsObjectLiteral()) {
    return AsObjectLiteral()->needs_initial_allocation_site();
  }
  DCHECK(IsRegExpLiteral());
  return false;
}

void MaterializedLiteral::BuildConstants(Isolate* isolate) {
  if (IsArrayLiteral()) {
    AsArrayLiteral()->BuildBoilerplateDescription(isolate);
    return;
  }
  if (IsObjectLiteral()) {
    AsObjectLiteral()->BuildBoilerplateDescription(isolate);
    return;
  }
  DCHECK(IsRegExpLiteral());
}

Handle<TemplateObjectDescription> GetTemplateObject::GetOrBuildDescription(
    Isolate* isolate) {
  Handle<FixedArray> raw_strings = isolate->factory()->NewFixedArray(
      this->raw_strings()->length(), AllocationType::kOld);
  bool raw_and_cooked_match = true;
  for (int i = 0; i < raw_strings->length(); ++i) {
    if (this->cooked_strings()->at(i) == nullptr ||
        *this->raw_strings()->at(i)->string() !=
            *this->cooked_strings()->at(i)->string()) {
      raw_and_cooked_match = false;
    }
    raw_strings->set(i, *this->raw_strings()->at(i)->string());
  }
  Handle<FixedArray> cooked_strings = raw_strings;
  if (!raw_and_cooked_match) {
    cooked_strings = isolate->factory()->NewFixedArray(
        this->cooked_strings()->length(), AllocationType::kOld);
    for (int i = 0; i < cooked_strings->length(); ++i) {
      if (this->cooked_strings()->at(i) != nullptr) {
        cooked_strings->set(i, *this->cooked_strings()->at(i)->string());
      } else {
        cooked_strings->set(i, ReadOnlyRoots(isolate).undefined_value());
      }
    }
  }
  return isolate->factory()->NewTemplateObjectDescription(raw_strings,
                                                          cooked_strings);
}

static bool IsCommutativeOperationWithSmiLiteral(Token::Value op) {
  // Add is not commutative due to potential for string addition.
  return op == Token::MUL || op == Token::BIT_AND || op == Token::BIT_OR ||
         op == Token::BIT_XOR;
}

// Check for the pattern: x + 1.
static bool MatchSmiLiteralOperation(Expression* left, Expression* right,
                                     Expression** expr, Smi* literal) {
  if (right->IsSmiLiteral()) {
    *expr = left;
    *literal = right->AsLiteral()->AsSmiLiteral();
    return true;
  }
  return false;
}

bool BinaryOperation::IsSmiLiteralOperation(Expression** subexpr,
                                            Smi* literal) {
  return MatchSmiLiteralOperation(left_, right_, subexpr, literal) ||
         (IsCommutativeOperationWithSmiLiteral(op()) &&
          MatchSmiLiteralOperation(right_, left_, subexpr, literal));
}

static bool IsTypeof(Expression* expr) {
  UnaryOperation* maybe_unary = expr->AsUnaryOperation();
  return maybe_unary != nullptr && maybe_unary->op() == Token::TYPEOF;
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
  return maybe_unary != nullptr && maybe_unary->op() == Token::VOID &&
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

Call::CallType Call::GetCallType() const {
  VariableProxy* proxy = expression()->AsVariableProxy();
  if (proxy != nullptr) {
    if (proxy->var()->IsUnallocated()) {
      return GLOBAL_CALL;
    } else if (proxy->var()->IsLookupSlot()) {
      // Calls going through 'with' always use VariableMode::kDynamic rather
      // than VariableMode::kDynamicLocal or VariableMode::kDynamicGlobal.
      return proxy->var()->mode() == VariableMode::kDynamic ? WITH_CALL
                                                            : OTHER_CALL;
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

  if (expression()->IsResolvedProperty()) {
    return RESOLVED_PROPERTY_CALL;
  }

  return OTHER_CALL;
}

CaseClause::CaseClause(Zone* zone, Expression* label,
                       const ScopedPtrList<Statement>& statements)
    : label_(label), statements_(0, nullptr) {
  statements.CopyTo(&statements_, zone);
}

bool Literal::IsPropertyName() const {
  if (type() != kString) return false;
  uint32_t index;
  return !string_->AsArrayIndex(&index);
}

bool Literal::ToUint32(uint32_t* value) const {
  switch (type()) {
    case kString:
      return string_->AsArrayIndex(value);
    case kSmi:
      if (smi_ < 0) return false;
      *value = static_cast<uint32_t>(smi_);
      return true;
    case kHeapNumber:
      return DoubleToUint32IfEqualToSelf(AsNumber(), value);
    default:
      return false;
  }
}

bool Literal::AsArrayIndex(uint32_t* value) const {
  return ToUint32(value) && *value != kMaxUInt32;
}

Handle<Object> Literal::BuildValue(Isolate* isolate) const {
  switch (type()) {
    case kSmi:
      return handle(Smi::FromInt(smi_), isolate);
    case kHeapNumber:
      return isolate->factory()->NewNumber(number_, AllocationType::kOld);
    case kString:
      return string_->string();
    case kSymbol:
      return isolate->factory()->home_object_symbol();
    case kBoolean:
      return isolate->factory()->ToBoolean(boolean_);
    case kNull:
      return isolate->factory()->null_value();
    case kUndefined:
      return isolate->factory()->undefined_value();
    case kTheHole:
      return isolate->factory()->the_hole_value();
    case kBigInt:
      // This should never fail: the parser will never create a BigInt
      // literal that cannot be allocated.
      return BigIntLiteral(isolate, bigint_.c_str()).ToHandleChecked();
  }
  UNREACHABLE();
}

bool Literal::ToBooleanIsTrue() const {
  switch (type()) {
    case kSmi:
      return smi_ != 0;
    case kHeapNumber:
      return DoubleToBoolean(number_);
    case kString:
      return !string_->IsEmpty();
    case kNull:
    case kUndefined:
      return false;
    case kBoolean:
      return boolean_;
    case kBigInt: {
      const char* bigint_str = bigint_.c_str();
      size_t length = strlen(bigint_str);
      DCHECK_GT(length, 0);
      if (length == 1 && bigint_str[0] == '0') return false;
      // Skip over any radix prefix; BigInts with length > 1 only
      // begin with zero if they include a radix.
      for (size_t i = (bigint_str[0] == '0') ? 2 : 0; i < length; ++i) {
        if (bigint_str[i] != '0') return true;
      }
      return false;
    }
    case kSymbol:
      return true;
    case kTheHole:
      UNREACHABLE();
  }
  UNREACHABLE();
}

uint32_t Literal::Hash() {
  return IsString() ? AsRawString()->Hash()
                    : ComputeLongHash(double_to_uint64(AsNumber()));
}


// static
bool Literal::Match(void* a, void* b) {
  Literal* x = static_cast<Literal*>(a);
  Literal* y = static_cast<Literal*>(b);
  return (x->IsString() && y->IsString() &&
          x->AsRawString() == y->AsRawString()) ||
         (x->IsNumber() && y->IsNumber() && x->AsNumber() == y->AsNumber());
}

Literal* AstNodeFactory::NewNumberLiteral(double number, int pos) {
  int int_value;
  if (DoubleToSmiInteger(number, &int_value)) {
    return NewSmiLiteral(int_value, pos);
  }
  return new (zone_) Literal(number, pos);
}

const char* CallRuntime::debug_name() {
#ifdef DEBUG
  return is_jsruntime() ? NameForNativeContextIntrinsicIndex(context_index_)
                        : function_->name;
#else
  return is_jsruntime() ? "(context function)" : function_->name;
#endif  // DEBUG
}

#define RETURN_LABELS(NodeType) \
  case k##NodeType:             \
    return static_cast<const NodeType*>(this)->labels();

ZonePtrList<const AstRawString>* BreakableStatement::labels() const {
  switch (node_type()) {
    BREAKABLE_NODE_LIST(RETURN_LABELS)
    ITERATION_NODE_LIST(RETURN_LABELS)
    default:
      UNREACHABLE();
  }
}

#undef RETURN_LABELS

}  // namespace internal
}  // namespace v8
