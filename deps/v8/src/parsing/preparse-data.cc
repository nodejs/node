// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/preparse-data.h"

#include <vector>

#include "src/ast/scopes.h"
#include "src/ast/variables.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/parsing/preparse-data-impl.h"
#include "src/parsing/preparser.h"

namespace v8 {
namespace internal {

namespace {

class ScopeCallsSloppyEvalField : public BitField<bool, 0, 1> {};
class InnerScopeCallsEvalField
    : public BitField<bool, ScopeCallsSloppyEvalField::kNext, 1> {};

class VariableMaybeAssignedField : public BitField8<bool, 0, 1> {};
class VariableContextAllocatedField
    : public BitField8<bool, VariableMaybeAssignedField::kNext, 1> {};

class LanguageField : public BitField8<LanguageMode, 0, 1> {};
class UsesSuperField : public BitField8<bool, LanguageField::kNext, 1> {};
STATIC_ASSERT(LanguageModeSize <= LanguageField::kNumValues);

}  // namespace

/*

  Internal data format for the backing store of PreparseDataBuilder and
  PreparseData::scope_data (on the heap):

  (Skippable function data:)
  ------------------------------------
  | scope_data_start (debug only)    |
  ------------------------------------
  | data for inner function n        |
  | ...                              |
  ------------------------------------
  | data for inner function 1        |
  | ...                              |
  ------------------------------------
  (Scope allocation data:)             << scope_data_start points here in debug
  ------------------------------------
  magic value (debug only)
  ------------------------------------
  scope positions (debug only)
  ------------------------------------
  | scope type << only in debug      |
  | eval                             |
  | ----------------------           |
  | | data for variables |           |
  | | ...                |           |
  | ----------------------           |
  ------------------------------------
  ------------------------------------
  | data for inner scope m           | << but not for function scopes
  | ...                              |
  ------------------------------------
  ...
  ------------------------------------
  | data for inner scope 1           |
  | ...                              |
  ------------------------------------

  PreparseData::child_data is an array of PreparseData objects, one
  for each skippable inner function.

  ConsumedPreparseData wraps a PreparseData and reads data from it.

 */

void PreparseDataBuilder::ByteData::WriteUint32(uint32_t data) {
#ifdef DEBUG
  // Save expected item size in debug mode.
  backing_store_.push_back(kUint32Size);
#endif
  backing_store_.push_back(data & 0xFF);
  backing_store_.push_back((data >> 8) & 0xFF);
  backing_store_.push_back((data >> 16) & 0xFF);
  backing_store_.push_back((data >> 24) & 0xFF);
  free_quarters_in_last_byte_ = 0;
}

#ifdef DEBUG
void PreparseDataBuilder::ByteData::OverwriteFirstUint32(uint32_t data) {
  auto it = backing_store_.begin();
  // Check that that position already holds an item of the expected size.
  DCHECK_GE(backing_store_.size(), kUint32Size);
  DCHECK_EQ(*it, kUint32Size);
  ++it;
  *it++ = data & 0xFF;
  *it++ = (data >> 8) & 0xFF;
  *it++ = (data >> 16) & 0xFF;
  *it++ = (data >> 24) & 0xFF;
}
#endif

void PreparseDataBuilder::ByteData::WriteUint8(uint8_t data) {
#ifdef DEBUG
  // Save expected item size in debug mode.
  backing_store_.push_back(kUint8Size);
#endif
  backing_store_.push_back(data);
  free_quarters_in_last_byte_ = 0;
}

void PreparseDataBuilder::ByteData::WriteQuarter(uint8_t data) {
  DCHECK_LE(data, 3);
  if (free_quarters_in_last_byte_ == 0) {
#ifdef DEBUG
    // Save a marker in debug mode.
    backing_store_.push_back(kQuarterMarker);
#endif
    backing_store_.push_back(0);
    free_quarters_in_last_byte_ = 3;
  } else {
    --free_quarters_in_last_byte_;
  }

  uint8_t shift_amount = free_quarters_in_last_byte_ * 2;
  DCHECK_EQ(backing_store_.back() & (3 << shift_amount), 0);
  backing_store_.back() |= (data << shift_amount);
}

Handle<PodArray<uint8_t>> PreparseDataBuilder::ByteData::Serialize(
    Isolate* isolate) {
  Handle<PodArray<uint8_t>> array = PodArray<uint8_t>::New(
      isolate, static_cast<int>(backing_store_.size()), TENURED);

  DisallowHeapAllocation no_gc;
  PodArray<uint8_t> raw_array = *array;

  int i = 0;
  for (uint8_t item : backing_store_) {
    raw_array->set(i++, item);
  }
  return array;
}

PreparseDataBuilder::PreparseDataBuilder(Zone* zone,
                                         PreparseDataBuilder* parent)
    : parent_(parent),
      byte_data_(new (zone) ByteData(zone)),
      data_for_inner_functions_(zone),
      bailed_out_(false) {
#ifdef DEBUG
  // Reserve space for scope_data_start, written later:
  byte_data_->WriteUint32(0);
#endif
}

void PreparseDataBuilder::DataGatheringScope::Start(
    DeclarationScope* function_scope) {
  PreparseDataBuilder* parent = preparser_->preparsed_scope_data_builder();
  Zone* main_zone = preparser_->main_zone();
  builder_ = new (main_zone) PreparseDataBuilder(main_zone, parent);
  preparser_->set_preparsed_scope_data_builder(builder_);
  function_scope->set_preparsed_scope_data_builder(builder_);
}

PreparseDataBuilder::DataGatheringScope::~DataGatheringScope() {
  if (builder_ == nullptr) return;
  PreparseDataBuilder* parent = builder_->parent_;
  if (parent != nullptr) {
    parent->data_for_inner_functions_.push_back(builder_->HasData() ? builder_
                                                                    : nullptr);
  }
  preparser_->set_preparsed_scope_data_builder(parent);
}

void PreparseDataBuilder::AddSkippableFunction(int start_position,
                                               int end_position,
                                               int num_parameters,
                                               int num_inner_functions,
                                               LanguageMode language_mode,
                                               bool uses_super_property) {
  if (bailed_out_) return;

  // Start position is used for a sanity check when consuming the data, we could
  // remove it in the future if we're very pressed for space but it's been good
  // at catching bugs in the wild so far.
  byte_data_->WriteUint32(start_position);
  byte_data_->WriteUint32(end_position);
  byte_data_->WriteUint32(num_parameters);
  byte_data_->WriteUint32(num_inner_functions);

  uint8_t language_and_super = LanguageField::encode(language_mode) |
                               UsesSuperField::encode(uses_super_property);

  byte_data_->WriteQuarter(language_and_super);
}

void PreparseDataBuilder::SaveScopeAllocationData(DeclarationScope* scope) {
  // The data contains a uint32 (reserved space for scope_data_start) and
  // function data items, kSkippableFunctionDataSize each.
  DCHECK_GE(byte_data_->size(), ByteData::kPlaceholderSize);
  DCHECK_LE(byte_data_->size(), std::numeric_limits<uint32_t>::max());
  DCHECK_EQ(byte_data_->size() % ByteData::kSkippableFunctionDataSize,
            ByteData::kPlaceholderSize);

  if (bailed_out_) return;
  // If there are no skippable inner functions, we don't need to save anything.
  if (!ContainsInnerFunctions()) return;

#ifdef DEBUG
  uint32_t scope_data_start = static_cast<uint32_t>(byte_data_->size());
  byte_data_->OverwriteFirstUint32(scope_data_start);

  // For a data integrity check, write a value between data about skipped inner
  // funcs and data about variables.
  byte_data_->WriteUint32(ByteData::kMagicValue);
  byte_data_->WriteUint32(scope->start_position());
  byte_data_->WriteUint32(scope->end_position());
#endif

  if (ScopeNeedsData(scope)) SaveDataForScope(scope);
}

bool PreparseDataBuilder::ContainsInnerFunctions() const {
  return byte_data_->size() > ByteData::kPlaceholderSize;
}

bool PreparseDataBuilder::HasData() const {
  return !bailed_out_ && ContainsInnerFunctions();
}

Handle<PreparseData> PreparseDataBuilder::Serialize(Isolate* isolate) {
  DCHECK(HasData());
  DCHECK(!ThisOrParentBailedOut());

  int child_data_length = static_cast<int>(data_for_inner_functions_.size());
  Handle<PreparseData> data =
      isolate->factory()->NewPreparseData(child_data_length);

  Handle<PodArray<uint8_t>> scope_data_array = byte_data_->Serialize(isolate);
  data->set_scope_data(*scope_data_array);

  int i = 0;
  for (const auto& item : data_for_inner_functions_) {
    if (item != nullptr) {
      Handle<PreparseData> child_data = item->Serialize(isolate);
      data->set_child_data(i, *child_data);
    } else {
      DCHECK(data->child_data(i)->IsNull());
    }
    i++;
  }

  return data;
}

ZonePreparseData* PreparseDataBuilder::Serialize(Zone* zone) {
  DCHECK(HasData());
  DCHECK(!ThisOrParentBailedOut());

  int child_length = static_cast<int>(data_for_inner_functions_.size());
  ZonePreparseData* result =
      new (zone) ZonePreparseData(zone, byte_data_, child_length);

  int i = 0;
  for (const auto& item : data_for_inner_functions_) {
    if (item != nullptr) {
      ZonePreparseData* child = item->Serialize(zone);
      result->set_child(i, child);
    }
    i++;
  }

  return result;
}

bool PreparseDataBuilder::ScopeNeedsData(Scope* scope) {
  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    // Default constructors don't need data (they cannot contain inner functions
    // defined by the user). Other functions do.
    return !IsDefaultConstructor(scope->AsDeclarationScope()->function_kind());
  }
  if (!scope->is_hidden()) {
    for (Variable* var : *scope->locals()) {
      if (IsDeclaredVariableMode(var->mode())) return true;
    }
  }
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    if (ScopeNeedsData(inner)) return true;
  }
  return false;
}

bool PreparseDataBuilder::ScopeIsSkippableFunctionScope(Scope* scope) {
  // Lazy non-arrow function scopes are skippable. Lazy functions are exactly
  // those Scopes which have their own PreparseDataBuilder object. This
  // logic ensures that the scope allocation data is consistent with the
  // skippable function data (both agree on where the lazy function boundaries
  // are).
  if (scope->scope_type() != ScopeType::FUNCTION_SCOPE) return false;
  DeclarationScope* declaration_scope = scope->AsDeclarationScope();
  return !declaration_scope->is_arrow_scope() &&
         declaration_scope->preparsed_scope_data_builder() != nullptr;
}

void PreparseDataBuilder::SaveDataForScope(Scope* scope) {
  DCHECK_NE(scope->end_position(), kNoSourcePosition);
  DCHECK(ScopeNeedsData(scope));

#ifdef DEBUG
  byte_data_->WriteUint8(scope->scope_type());
#endif

  uint8_t eval =
      ScopeCallsSloppyEvalField::encode(
          scope->is_declaration_scope() &&
          scope->AsDeclarationScope()->calls_sloppy_eval()) |
      InnerScopeCallsEvalField::encode(scope->inner_scope_calls_eval());
  byte_data_->WriteUint8(eval);

  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    Variable* function = scope->AsDeclarationScope()->function_var();
    if (function != nullptr) SaveDataForVariable(function);
  }

  for (Variable* var : *scope->locals()) {
    if (IsDeclaredVariableMode(var->mode())) SaveDataForVariable(var);
  }

  SaveDataForInnerScopes(scope);
}

void PreparseDataBuilder::SaveDataForVariable(Variable* var) {
#ifdef DEBUG
  // Store the variable name in debug mode; this way we can check that we
  // restore data to the correct variable.
  const AstRawString* name = var->raw_name();
  byte_data_->WriteUint8(name->is_one_byte());
  byte_data_->WriteUint32(name->length());
  for (int i = 0; i < name->length(); ++i) {
    byte_data_->WriteUint8(name->raw_data()[i]);
  }
#endif
  byte variable_data = VariableMaybeAssignedField::encode(
                           var->maybe_assigned() == kMaybeAssigned) |
                       VariableContextAllocatedField::encode(
                           var->has_forced_context_allocation());
  byte_data_->WriteQuarter(variable_data);
}

void PreparseDataBuilder::SaveDataForInnerScopes(Scope* scope) {
  // Inner scopes are stored in the reverse order, but we'd like to write the
  // data in the logical order. There might be many inner scopes, so we don't
  // want to recurse here.
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    if (ScopeIsSkippableFunctionScope(inner)) {
      // Don't save data about function scopes, since they'll have their own
      // PreparseDataBuilder where their data is saved.
      DCHECK_NOT_NULL(
          inner->AsDeclarationScope()->preparsed_scope_data_builder());
      continue;
    }
    if (!ScopeNeedsData(inner)) continue;
    SaveDataForScope(inner);
  }
}

class BuilderProducedPreparseData final : public ProducedPreparseData {
 public:
  explicit BuilderProducedPreparseData(PreparseDataBuilder* builder)
      : builder_(builder) {
    DCHECK(builder->HasData());
  }

  Handle<PreparseData> Serialize(Isolate* isolate) final {
    return builder_->Serialize(isolate);
  }

  ZonePreparseData* Serialize(Zone* zone) final {
    return builder_->Serialize(zone);
  };

 private:
  PreparseDataBuilder* builder_;
};

class OnHeapProducedPreparseData final : public ProducedPreparseData {
 public:
  explicit OnHeapProducedPreparseData(Handle<PreparseData> data)
      : data_(data) {}

  Handle<PreparseData> Serialize(Isolate* isolate) final {
    DCHECK(!data_->is_null());
    return data_;
  }

  ZonePreparseData* Serialize(Zone* zone) final {
    // Not required.
    UNREACHABLE();
  };

 private:
  Handle<PreparseData> data_;
};

class ZoneProducedPreparseData final : public ProducedPreparseData {
 public:
  explicit ZoneProducedPreparseData(ZonePreparseData* data) : data_(data) {}

  Handle<PreparseData> Serialize(Isolate* isolate) final {
    return data_->Serialize(isolate);
  }

  ZonePreparseData* Serialize(Zone* zone) final { return data_; };

 private:
  ZonePreparseData* data_;
};

ProducedPreparseData* ProducedPreparseData::For(PreparseDataBuilder* builder,
                                                Zone* zone) {
  return new (zone) BuilderProducedPreparseData(builder);
}

ProducedPreparseData* ProducedPreparseData::For(Handle<PreparseData> data,
                                                Zone* zone) {
  return new (zone) OnHeapProducedPreparseData(data);
}

ProducedPreparseData* ProducedPreparseData::For(ZonePreparseData* data,
                                                Zone* zone) {
  return new (zone) ZoneProducedPreparseData(data);
}

template <class Data>
ProducedPreparseData*
BaseConsumedPreparseData<Data>::GetDataForSkippableFunction(
    Zone* zone, int start_position, int* end_position, int* num_parameters,
    int* num_inner_functions, bool* uses_super_property,
    LanguageMode* language_mode) {
  // The skippable function *must* be the next function in the data. Use the
  // start position as a sanity check.
  typename ByteData::ReadingScope reading_scope(this);
  CHECK(scope_data_->HasRemainingBytes(ByteData::kSkippableFunctionDataSize));
  int start_position_from_data = scope_data_->ReadUint32();
  CHECK_EQ(start_position, start_position_from_data);

  *end_position = scope_data_->ReadUint32();
  DCHECK_GT(*end_position, start_position);
  *num_parameters = scope_data_->ReadUint32();
  *num_inner_functions = scope_data_->ReadUint32();

  uint8_t language_and_super = scope_data_->ReadQuarter();
  *language_mode = LanguageMode(LanguageField::decode(language_and_super));
  *uses_super_property = UsesSuperField::decode(language_and_super);

  // Retrieve the corresponding PreparseData and associate it to the
  // skipped function. If the skipped functions contains inner functions, those
  // can be skipped when the skipped function is eagerly parsed.
  return GetChildData(zone, child_index_++);
}

template <class Data>
void BaseConsumedPreparseData<Data>::RestoreScopeAllocationData(
    DeclarationScope* scope) {
  DCHECK_EQ(scope->scope_type(), ScopeType::FUNCTION_SCOPE);
  typename ByteData::ReadingScope reading_scope(this);

#ifdef DEBUG
  int magic_value_from_data = scope_data_->ReadUint32();
  // Check that we've consumed all inner function data.
  DCHECK_EQ(magic_value_from_data, ByteData::kMagicValue);

  int start_position_from_data = scope_data_->ReadUint32();
  int end_position_from_data = scope_data_->ReadUint32();
  DCHECK_EQ(start_position_from_data, scope->start_position());
  DCHECK_EQ(end_position_from_data, scope->end_position());
#endif

  RestoreDataForScope(scope);

  // Check that we consumed all scope data.
  DCHECK_EQ(scope_data_->RemainingBytes(), 0);
}

template <typename Data>
void BaseConsumedPreparseData<Data>::RestoreDataForScope(Scope* scope) {
  if (scope->is_declaration_scope() &&
      scope->AsDeclarationScope()->is_skipped_function()) {
    return;
  }

  // It's possible that scope is not present in the data at all (since PreParser
  // doesn't create the corresponding scope). In this case, the Scope won't
  // contain any variables for which we need the data.
  if (!PreparseDataBuilder::ScopeNeedsData(scope)) return;

  // scope_type is stored only in debug mode.
  DCHECK_EQ(scope_data_->ReadUint8(), scope->scope_type());

  CHECK(scope_data_->HasRemainingBytes(ByteData::kUint8Size));
  uint32_t eval = scope_data_->ReadUint8();
  if (ScopeCallsSloppyEvalField::decode(eval)) scope->RecordEvalCall();
  if (InnerScopeCallsEvalField::decode(eval)) scope->RecordInnerScopeEvalCall();

  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    Variable* function = scope->AsDeclarationScope()->function_var();
    if (function != nullptr) RestoreDataForVariable(function);
  }

  for (Variable* var : *scope->locals()) {
    if (IsDeclaredVariableMode(var->mode())) RestoreDataForVariable(var);
  }

  RestoreDataForInnerScopes(scope);
}

template <typename Data>
void BaseConsumedPreparseData<Data>::RestoreDataForVariable(Variable* var) {
#ifdef DEBUG
  const AstRawString* name = var->raw_name();
  bool data_one_byte = scope_data_->ReadUint8();
  DCHECK_IMPLIES(name->is_one_byte(), data_one_byte);
  DCHECK_EQ(scope_data_->ReadUint32(), static_cast<uint32_t>(name->length()));
  if (!name->is_one_byte() && data_one_byte) {
    // It's possible that "name" is a two-byte representation of the string
    // stored in the data.
    for (int i = 0; i < 2 * name->length(); i += 2) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
      DCHECK_EQ(scope_data_->ReadUint8(), name->raw_data()[i]);
      DCHECK_EQ(0, name->raw_data()[i + 1]);
#else
      DCHECK_EQ(scope_data_->ReadUint8(), name->raw_data()[i + 1]);
      DCHECK_EQ(0, name->raw_data()[i]);
#endif  // V8_TARGET_LITTLE_ENDIAN
    }
  } else {
    for (int i = 0; i < name->length(); ++i) {
      DCHECK_EQ(scope_data_->ReadUint8(), name->raw_data()[i]);
    }
  }
#endif
  uint8_t variable_data = scope_data_->ReadQuarter();
  if (VariableMaybeAssignedField::decode(variable_data)) {
    var->set_maybe_assigned();
  }
  if (VariableContextAllocatedField::decode(variable_data)) {
    var->set_is_used();
    var->ForceContextAllocation();
  }
}

template <typename Data>
void BaseConsumedPreparseData<Data>::RestoreDataForInnerScopes(Scope* scope) {
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    RestoreDataForScope(inner);
  }
}

#ifdef DEBUG
template <class Data>
void BaseConsumedPreparseData<Data>::VerifyDataStart() {
  typename ByteData::ReadingScope reading_scope(this);
  int scope_data_start = scope_data_->ReadUint32();
  scope_data_->SetPosition(scope_data_start);
  DCHECK_EQ(scope_data_->ReadUint32(), ByteData::kMagicValue);
  // The first data item is scope_data_start. Skip over it.
  scope_data_->SetPosition(ByteData::kPlaceholderSize);
}
#endif

PodArray<uint8_t> OnHeapConsumedPreparseData::GetScopeData() {
  return data_->scope_data();
}

ProducedPreparseData* OnHeapConsumedPreparseData::GetChildData(
    Zone* zone, int child_index) {
  CHECK_GT(data_->length(), child_index);
  Object child_data = data_->child_data(child_index);
  if (!child_data->IsPreparseData()) return nullptr;
  Handle<PreparseData> child_data_handle(PreparseData::cast(child_data),
                                         isolate_);
  return ProducedPreparseData::For(child_data_handle, zone);
}

OnHeapConsumedPreparseData::OnHeapConsumedPreparseData(
    Isolate* isolate, Handle<PreparseData> data)
    : BaseConsumedPreparseData<PodArray<uint8_t>>(),
      isolate_(isolate),
      data_(data) {
  DCHECK_NOT_NULL(isolate);
  DCHECK(data->IsPreparseData());
#ifdef DEBUG
  VerifyDataStart();
#endif
}

ZonePreparseData::ZonePreparseData(Zone* zone,
                                   PreparseDataBuilder::ByteData* byte_data,
                                   int child_length)
    : byte_data_(byte_data->begin(), byte_data->end(), zone),
      children_(child_length, zone) {}

Handle<PreparseData> ZonePreparseData::Serialize(Isolate* isolate) {
  int child_data_length = child_length();
  Handle<PreparseData> result =
      isolate->factory()->NewPreparseData(child_data_length);

  Handle<PodArray<uint8_t>> scope_data_array = PodArray<uint8_t>::New(
      isolate, static_cast<int>(byte_data()->size()), TENURED);
  scope_data_array->copy_in(0, byte_data()->data(),
                            static_cast<int>(byte_data()->size()));
  result->set_scope_data(*scope_data_array);

  for (int i = 0; i < child_data_length; i++) {
    ZonePreparseData* child = get_child(i);
    if (child) {
      Handle<PreparseData> child_data = child->Serialize(isolate);
      result->set_child_data(i, *child_data);
    }
  }
  return result;
}

ZoneConsumedPreparseData::ZoneConsumedPreparseData(Zone* zone,
                                                   ZonePreparseData* data)
    : data_(data), scope_data_wrapper_(data_->byte_data()) {
#ifdef DEBUG
  VerifyDataStart();
#endif
}

ZoneVectorWrapper ZoneConsumedPreparseData::GetScopeData() {
  return scope_data_wrapper_;
}

ProducedPreparseData* ZoneConsumedPreparseData::GetChildData(Zone* zone,
                                                             int child_index) {
  CHECK_GT(data_->child_length(), child_index);
  ZonePreparseData* child_data = data_->get_child(child_index);
  if (child_data == nullptr) return nullptr;
  return ProducedPreparseData::For(child_data, zone);
}

std::unique_ptr<ConsumedPreparseData> ConsumedPreparseData::For(
    Isolate* isolate, Handle<PreparseData> data) {
  DCHECK(!data.is_null());
  return base::make_unique<OnHeapConsumedPreparseData>(isolate, data);
}

std::unique_ptr<ConsumedPreparseData> ConsumedPreparseData::For(
    Zone* zone, ZonePreparseData* data) {
  if (data == nullptr) return {};
  return base::make_unique<ZoneConsumedPreparseData>(zone, data);
}

}  // namespace internal
}  // namespace v8
