// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/preparsed-scope-data.h"

#include "src/ast/scopes.h"
#include "src/ast/variables.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

bool PreParsedScopeData::HasVariablesWhichNeedAllocationData(Scope* scope) {
  if (!scope->is_hidden()) {
    for (Variable* var : *scope->locals()) {
      if (var->mode() == VAR || var->mode() == LET || var->mode() == CONST) {
        return true;
      }
    }
  }
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    if (HasVariablesWhichNeedAllocationData(inner)) {
      return true;
    }
  }
  return false;
}

PreParsedScopeData::ScopeScope::ScopeScope(PreParsedScopeData* data,
                                           ScopeType scope_type,
                                           int start_position, int end_position)
    : data_(data), previous_scope_(data->current_scope_) {
  data->current_scope_ = this;
  data->backing_store_.push_back(scope_type);
  data->backing_store_.push_back(start_position);
  data->backing_store_.push_back(end_position);
  // Reserve space for variable and inner scope count (we don't know yet how
  // many will be added).
  index_in_data_ = data->backing_store_.size();
  data->backing_store_.push_back(-1);
  data->backing_store_.push_back(-1);
}

PreParsedScopeData::ScopeScope::~ScopeScope() {
  data_->current_scope_ = previous_scope_;
  if (got_data_) {
    DCHECK_GT(variable_count_ + inner_scope_count_, 0);
    if (previous_scope_ != nullptr) {
      previous_scope_->got_data_ = true;
      ++previous_scope_->inner_scope_count_;
    }
    data_->backing_store_[index_in_data_] = inner_scope_count_;
    data_->backing_store_[index_in_data_ + 1] = variable_count_;
  } else {
    // No interesting data for this scope (or its children); remove from the
    // data.
    DCHECK_EQ(data_->backing_store_.size(), index_in_data_ + 2);
    DCHECK_GE(index_in_data_, 3);
    DCHECK_EQ(variable_count_, 0);
    data_->backing_store_.erase(
        data_->backing_store_.begin() + index_in_data_ - 3,
        data_->backing_store_.end());
  }
}

void PreParsedScopeData::ScopeScope::MaybeAddVariable(Variable* var) {
  if (var->mode() == VAR || var->mode() == LET || var->mode() == CONST) {
#ifdef DEBUG
    // For tests (which check that the data is about the same variables).
    const AstRawString* name = var->raw_name();
    data_->backing_store_.push_back(name->length());
    for (int i = 0; i < name->length(); ++i) {
      data_->backing_store_.push_back(name->raw_data()[i]);
    }
#endif
    data_->backing_store_.push_back(var->location());
    data_->backing_store_.push_back(var->maybe_assigned());
    ++variable_count_;
    got_data_ = true;
  }
}

}  // namespace internal
}  // namespace v8
