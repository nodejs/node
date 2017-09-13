// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/preparsed-scope-data.h"

#include "src/ast/scopes.h"
#include "src/ast/variables.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/parsing/preparser.h"

namespace v8 {
namespace internal {

namespace {

class ScopeCallsEvalField : public BitField<bool, 0, 1> {};
class InnerScopeCallsEvalField
    : public BitField<bool, ScopeCallsEvalField::kNext, 1> {};

class VariableIsUsedField : public BitField16<bool, 0, 1> {};
class VariableMaybeAssignedField
    : public BitField16<bool, VariableIsUsedField::kNext, 1> {};
class VariableContextAllocatedField
    : public BitField16<bool, VariableMaybeAssignedField::kNext, 1> {};

const int kMagicValue = 0xc0de0de;

enum SkippableFunctionDataOffsets {
  kStartPosition,
  kEndPosition,
  kNumParameters,
  kNumInnerFunctions,
  kLanguageAndSuper,
  kSize
};

STATIC_ASSERT(LANGUAGE_END == 2);
class LanguageField : public BitField<int, 0, 1> {};
class UsesSuperField : public BitField<bool, LanguageField::kNext, 1> {};

}  // namespace

/*

  Internal data format for the backing store of ProducedPreparsedScopeData:

  (Skippable function data:)
  ------------------------------------
  | data for inner function 1        |
  | ...                              |
  ------------------------------------
  | data for inner function n        |
  | ...                              |
  ------------------------------------
  (Scope allocation data:)
  ------------------------------------
  magic value
  ------------------------------------
  scope positions
  ------------------------------------
  | scope type << only in debug      |
  | eval                             |
  | ----------------------           |
  | | data for variables |           |
  | | ...                |           |
  | ----------------------           |
  ------------------------------------
  ------------------------------------
  | data for inner scope 1           | << but not for function scopes
  | ...                              |
  ------------------------------------
  ...
  ------------------------------------
  | data for inner scope m           |
  | ...                              |
  ------------------------------------


  Data format for PreParsedScopeData (on the heap):

  PreParsedScopeData::scope_data:

  ------------------------------------
  | scope_data_start                 |
  ------------------------------------
  | Skippable function data          |
  | (see above)                      |
  | ...                              |
  ------------------------------------
  ------------------------------------
  | Scope allocation data            | << scope_data_start points here
  | (see above)                      |
  | ...                              |
  ------------------------------------

  PreParsedScopeData::child_data is an array of PreParsedScopeData objects, one
  for each skippable inner function.


  ConsumedPreParsedScopeData wraps a PreParsedScopeData and reads data from it.

 */

ProducedPreParsedScopeData::DataGatheringScope::DataGatheringScope(
    DeclarationScope* function_scope, PreParser* preparser)
    : function_scope_(function_scope),
      preparser_(preparser),
      parent_data_(preparser->produced_preparsed_scope_data()) {
  if (FLAG_experimental_preparser_scope_analysis) {
    Zone* main_zone = preparser->main_zone();
    auto* new_data = new (main_zone) ProducedPreParsedScopeData(main_zone);
    if (parent_data_ != nullptr) {
      parent_data_->data_for_inner_functions_.push_back(new_data);
    }
    preparser->set_produced_preparsed_scope_data(new_data);
    function_scope->set_produced_preparsed_scope_data(new_data);
  }
}

ProducedPreParsedScopeData::DataGatheringScope::~DataGatheringScope() {
  if (FLAG_experimental_preparser_scope_analysis) {
    preparser_->set_produced_preparsed_scope_data(parent_data_);
  }
}

void ProducedPreParsedScopeData::DataGatheringScope::MarkFunctionAsSkippable(
    int end_position, int num_inner_functions) {
  DCHECK(FLAG_experimental_preparser_scope_analysis);
  DCHECK_NOT_NULL(parent_data_);
  parent_data_->AddSkippableFunction(
      function_scope_->start_position(), end_position,
      function_scope_->num_parameters(), num_inner_functions,
      function_scope_->language_mode(), function_scope_->uses_super_property());
}

void ProducedPreParsedScopeData::AddSkippableFunction(
    int start_position, int end_position, int num_parameters,
    int num_inner_functions, LanguageMode language_mode,
    bool uses_super_property) {
  DCHECK(FLAG_experimental_preparser_scope_analysis);
  DCHECK_EQ(scope_data_start_, -1);
  DCHECK(previously_produced_preparsed_scope_data_.is_null());

  size_t current_size = backing_store_.size();
  backing_store_.resize(current_size + SkippableFunctionDataOffsets::kSize);
  backing_store_[current_size + SkippableFunctionDataOffsets::kStartPosition] =
      start_position;
  backing_store_[current_size + SkippableFunctionDataOffsets::kEndPosition] =
      end_position;
  backing_store_[current_size + SkippableFunctionDataOffsets::kNumParameters] =
      num_parameters;
  backing_store_[current_size +
                 SkippableFunctionDataOffsets::kNumInnerFunctions] =
      num_inner_functions;

  uint32_t language_and_super = LanguageField::encode(language_mode) |
                                UsesSuperField::encode(uses_super_property);

  backing_store_[current_size +
                 SkippableFunctionDataOffsets::kLanguageAndSuper] =
      language_and_super;
}

void ProducedPreParsedScopeData::SaveScopeAllocationData(
    DeclarationScope* scope) {
  DCHECK(FLAG_experimental_preparser_scope_analysis);
  DCHECK(previously_produced_preparsed_scope_data_.is_null());
  DCHECK_EQ(scope_data_start_, -1);
  DCHECK_EQ(backing_store_.size() % SkippableFunctionDataOffsets::kSize, 0);

  scope_data_start_ = static_cast<int>(backing_store_.size());

  // If there are no skippable inner functions, we don't need to save anything.
  if (backing_store_.size() == 0) {
    return;
  }

  // For sanity checks.
  backing_store_.push_back(kMagicValue);
  backing_store_.push_back(scope->start_position());
  backing_store_.push_back(scope->end_position());

  // For a data integrity check, write a value between data about skipped inner
  // funcs and data about variables.
  SaveDataForScope(scope);
}

MaybeHandle<PreParsedScopeData> ProducedPreParsedScopeData::Serialize(
    Isolate* isolate) const {
  if (!previously_produced_preparsed_scope_data_.is_null()) {
    DCHECK_EQ(backing_store_.size(), 0);
    DCHECK_EQ(data_for_inner_functions_.size(), 0);
    return previously_produced_preparsed_scope_data_;
  }
  // FIXME(marja): save space by using a byte array and converting
  // function data to bytes.
  size_t length = backing_store_.size();
  if (length == 0) {
    return MaybeHandle<PreParsedScopeData>();
  }

  Handle<PodArray<uint32_t>> data_array =
      PodArray<uint32_t>::New(isolate, static_cast<int>(length + 1), TENURED);

  DCHECK_GE(scope_data_start_, 0);
  data_array->set(0, scope_data_start_);
  {
    int i = 1;
    for (const auto& item : backing_store_) {
      data_array->set(i++, item);
    }
  }

  Handle<PreParsedScopeData> data = isolate->factory()->NewPreParsedScopeData();

  int child_data_length = static_cast<int>(data_for_inner_functions_.size());
  if (child_data_length == 0) {
    data->set_child_data(*(isolate->factory()->empty_fixed_array()));
  } else {
    Handle<FixedArray> child_array =
        isolate->factory()->NewFixedArray(child_data_length, TENURED);
    int i = 0;
    for (const auto& item : data_for_inner_functions_) {
      MaybeHandle<PreParsedScopeData> maybe_child_data =
          item->Serialize(isolate);
      if (maybe_child_data.is_null()) {
        child_array->set(i++, *(isolate->factory()->null_value()));
      } else {
        Handle<PreParsedScopeData> child_data =
            maybe_child_data.ToHandleChecked();
        child_array->set(i++, *child_data);
      }
    }
    data->set_child_data(*child_array);
  }

  data->set_scope_data(*data_array);
  return data;
}

bool ProducedPreParsedScopeData::ScopeNeedsData(Scope* scope) {
  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    // Default constructors don't need data (they cannot contain inner functions
    // defined by the user). Other functions do.
    return !IsDefaultConstructor(scope->AsDeclarationScope()->function_kind());
  }
  if (!scope->is_hidden()) {
    for (Variable* var : *scope->locals()) {
      if (IsDeclaredVariableMode(var->mode())) {
        return true;
      }
    }
  }
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    if (ScopeNeedsData(inner)) {
      return true;
    }
  }
  return false;
}

bool ProducedPreParsedScopeData::ScopeIsSkippableFunctionScope(Scope* scope) {
  // Lazy non-arrow function scopes are skippable. Lazy functions are exactly
  // those Scopes which have their own ProducedPreParsedScopeData object. This
  // logic ensures that the scope allocation data is consistent with the
  // skippable function data (both agree on where the lazy function boundaries
  // are).
  if (scope->scope_type() != ScopeType::FUNCTION_SCOPE) {
    return false;
  }
  DeclarationScope* declaration_scope = scope->AsDeclarationScope();
  return !declaration_scope->is_arrow_scope() &&
         declaration_scope->produced_preparsed_scope_data() != nullptr;
}

void ProducedPreParsedScopeData::SaveDataForScope(Scope* scope) {
  DCHECK_NE(scope->end_position(), kNoSourcePosition);

  // We're not trying to save data for default constructors because the
  // PreParser doesn't construct them.
  DCHECK_IMPLIES(scope->scope_type() == ScopeType::FUNCTION_SCOPE,
                 (scope->AsDeclarationScope()->function_kind() &
                  kDefaultConstructor) == 0);

  if (!ScopeNeedsData(scope)) {
    return;
  }

#ifdef DEBUG
  backing_store_.push_back(scope->scope_type());
#endif

  uint32_t eval =
      ScopeCallsEvalField::encode(scope->calls_eval()) |
      InnerScopeCallsEvalField::encode(scope->inner_scope_calls_eval());
  backing_store_.push_back(eval);

  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    Variable* function = scope->AsDeclarationScope()->function_var();
    if (function != nullptr) {
      SaveDataForVariable(function);
    }
  }

  for (Variable* var : *scope->locals()) {
    if (IsDeclaredVariableMode(var->mode())) {
      SaveDataForVariable(var);
    }
  }

  SaveDataForInnerScopes(scope);
}

void ProducedPreParsedScopeData::SaveDataForVariable(Variable* var) {
#ifdef DEBUG
  // Store the variable name in debug mode; this way we can check that we
  // restore data to the correct variable.
  const AstRawString* name = var->raw_name();
  backing_store_.push_back(name->length());
  for (int i = 0; i < name->length(); ++i) {
    backing_store_.push_back(name->raw_data()[i]);
  }
#endif
  // FIXME(marja): Only 3 bits needed, not a full byte.
  byte variable_data = VariableIsUsedField::encode(var->is_used()) |
                       VariableMaybeAssignedField::encode(
                           var->maybe_assigned() == kMaybeAssigned) |
                       VariableContextAllocatedField::encode(
                           var->has_forced_context_allocation());

  backing_store_.push_back(variable_data);
}

void ProducedPreParsedScopeData::SaveDataForInnerScopes(Scope* scope) {
  // Inner scopes are stored in the reverse order, but we'd like to write the
  // data in the logical order. There might be many inner scopes, so we don't
  // want to recurse here.
  std::vector<Scope*> scopes;
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    if (ScopeIsSkippableFunctionScope(inner)) {
      // Don't save data about function scopes, since they'll have their own
      // ProducedPreParsedScopeData where their data is saved.
      DCHECK(inner->AsDeclarationScope()->produced_preparsed_scope_data() !=
             nullptr);
      continue;
    }
    scopes.push_back(inner);
  }
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    SaveDataForScope(*it);
  }
}

void ConsumedPreParsedScopeData::SetData(Handle<PreParsedScopeData> data) {
  DCHECK(data->IsPreParsedScopeData());
  data_ = data;
#ifdef DEBUG
  DisallowHeapAllocation no_gc;
  PodArray<uint32_t>* scope_data = data_->scope_data();
  DCHECK_GT(scope_data->length(), 2);
  DCHECK_EQ(scope_data->get(scope_data->get(0) + 1), kMagicValue);
#endif
}

ProducedPreParsedScopeData*
ConsumedPreParsedScopeData::GetDataForSkippableFunction(
    Zone* zone, int start_position, int* end_position, int* num_parameters,
    int* num_inner_functions, bool* uses_super_property,
    LanguageMode* language_mode) {
  DisallowHeapAllocation no_gc;
  PodArray<uint32_t>* scope_data = data_->scope_data();

  // The skippable function *must* be the next function in the data. Use the
  // start position as a sanity check.
  CHECK_GE(scope_data->length(), index_ + SkippableFunctionDataOffsets::kSize);
  int start_position_from_data =
      scope_data->get(index_ + SkippableFunctionDataOffsets::kStartPosition);
  CHECK_EQ(start_position, start_position_from_data);

  *end_position =
      scope_data->get(index_ + SkippableFunctionDataOffsets::kEndPosition);
  DCHECK_GT(*end_position, start_position);
  *num_parameters =
      scope_data->get(index_ + SkippableFunctionDataOffsets::kNumParameters);
  *num_inner_functions = scope_data->get(
      index_ + SkippableFunctionDataOffsets::kNumInnerFunctions);

  int language_and_super =
      scope_data->get(index_ + SkippableFunctionDataOffsets::kLanguageAndSuper);
  *language_mode = LanguageMode(LanguageField::decode(language_and_super));
  *uses_super_property = UsesSuperField::decode(language_and_super);

  index_ += SkippableFunctionDataOffsets::kSize;

  // Retrieve the corresponding PreParsedScopeData and associate it to the
  // skipped function. If the skipped functions contains inner functions, those
  // can be skipped when the skipped function is eagerly parsed.
  FixedArray* children = data_->child_data();
  CHECK_GT(children->length(), child_index_);
  Object* child_data = children->get(child_index_++);
  if (!child_data->IsPreParsedScopeData()) {
    return nullptr;
  }
  Handle<PreParsedScopeData> child_data_handle(
      PreParsedScopeData::cast(child_data));
  return new (zone) ProducedPreParsedScopeData(child_data_handle, zone);
}

void ConsumedPreParsedScopeData::RestoreScopeAllocationData(
    DeclarationScope* scope) {
  DCHECK(FLAG_experimental_preparser_scope_analysis);
  DCHECK_EQ(scope->scope_type(), ScopeType::FUNCTION_SCOPE);
  DCHECK(!data_.is_null());

  DisallowHeapAllocation no_gc;
  PodArray<uint32_t>* scope_data = data_->scope_data();
  int magic_value_from_data = scope_data->get(index_++);
  // Check that we've consumed all inner function data.
  CHECK_EQ(magic_value_from_data, kMagicValue);

  int start_position_from_data = scope_data->get(index_++);
  int end_position_from_data = scope_data->get(index_++);
  CHECK_EQ(start_position_from_data, scope->start_position());
  CHECK_EQ(end_position_from_data, scope->end_position());

  RestoreData(scope, scope_data);

  // Check that we consumed all scope data.
  DCHECK_EQ(index_, scope_data->length());
}

void ConsumedPreParsedScopeData::SkipFunctionDataForTesting() {
  DCHECK_EQ(index_, 1);
  DisallowHeapAllocation no_gc;
  PodArray<uint32_t>* scope_data = data_->scope_data();
  DCHECK_GT(scope_data->length(), 2);
  index_ = scope_data->get(0) + 1;
  DCHECK_EQ(scope_data->get(index_), kMagicValue);
}

void ConsumedPreParsedScopeData::RestoreData(Scope* scope,
                                             PodArray<uint32_t>* scope_data) {
  if (scope->is_declaration_scope() &&
      scope->AsDeclarationScope()->is_skipped_function()) {
    return;
  }

  // It's possible that scope is not present in the data at all (since PreParser
  // doesn't create the corresponding scope). In this case, the Scope won't
  // contain any variables for which we need the data.
  if (!ProducedPreParsedScopeData::ScopeNeedsData(scope)) {
    return;
  }

  // scope_type is stored only in debug mode.
  CHECK_GE(scope_data->length(), index_ + 1);
  DCHECK_GE(scope_data->length(), index_ + 2);
  DCHECK_EQ(scope_data->get(index_++), scope->scope_type());

  uint32_t eval = scope_data->get(index_++);
  if (ScopeCallsEvalField::decode(eval)) {
    scope->RecordEvalCall();
  }
  if (InnerScopeCallsEvalField::decode(eval)) {
    scope->RecordInnerScopeEvalCall();
  }

  if (scope->scope_type() == ScopeType::FUNCTION_SCOPE) {
    Variable* function = scope->AsDeclarationScope()->function_var();
    if (function != nullptr) {
      RestoreDataForVariable(function, scope_data);
    }
  }

  for (Variable* var : *scope->locals()) {
    if (IsDeclaredVariableMode(var->mode())) {
      RestoreDataForVariable(var, scope_data);
    }
  }

  RestoreDataForInnerScopes(scope, scope_data);
}

void ConsumedPreParsedScopeData::RestoreDataForVariable(
    Variable* var, PodArray<uint32_t>* scope_data) {
#ifdef DEBUG
  const AstRawString* name = var->raw_name();
  DCHECK_GT(scope_data->length(), index_ + name->length());
  DCHECK_EQ(scope_data->get(index_++), static_cast<uint32_t>(name->length()));
  for (int i = 0; i < name->length(); ++i) {
    DCHECK_EQ(scope_data->get(index_++), name->raw_data()[i]);
  }
#endif
  CHECK_GT(scope_data->length(), index_);
  byte variable_data = scope_data->get(index_++);
  if (VariableIsUsedField::decode(variable_data)) {
    var->set_is_used();
  }
  if (VariableMaybeAssignedField::decode(variable_data)) {
    var->set_maybe_assigned();
  }
  if (VariableContextAllocatedField::decode(variable_data)) {
    var->ForceContextAllocation();
  }
}

void ConsumedPreParsedScopeData::RestoreDataForInnerScopes(
    Scope* scope, PodArray<uint32_t>* scope_data) {
  std::vector<Scope*> scopes;
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    scopes.push_back(inner);
  }
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    RestoreData(*it, scope_data);
  }
}

}  // namespace internal
}  // namespace v8
