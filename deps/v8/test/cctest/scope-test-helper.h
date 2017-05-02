// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_SCOPE_TEST_HELPER_H_
#define V8_CCTEST_SCOPE_TEST_HELPER_H_

#include "src/ast/scopes.h"
#include "src/ast/variables.h"

namespace v8 {
namespace internal {

class ScopeTestHelper {
 public:
  static bool MustAllocateInContext(Variable* var) {
    return var->scope()->MustAllocateInContext(var);
  }

  static void CompareScopeToData(Scope* scope, const PreParsedScopeData* data,
                                 size_t& index, bool precise_maybe_assigned) {
    CHECK(PreParsedScopeData::HasVariablesWhichNeedAllocationData(scope));
    CHECK_GT(data->backing_store_.size(), index + 4);
    CHECK_EQ(data->backing_store_[index++], scope->scope_type());
    CHECK_EQ(data->backing_store_[index++], scope->start_position());
    CHECK_EQ(data->backing_store_[index++], scope->end_position());

    int inner_scope_count = 0;
    for (Scope* inner = scope->inner_scope(); inner != nullptr;
         inner = inner->sibling()) {
      if (PreParsedScopeData::HasVariablesWhichNeedAllocationData(inner)) {
        ++inner_scope_count;
      }
    }
    CHECK_EQ(data->backing_store_[index++], inner_scope_count);

    int variable_count = 0;
    for (Variable* local : scope->locals_) {
      if (local->mode() == VAR || local->mode() == LET ||
          local->mode() == CONST) {
        ++variable_count;
      }
    }

    CHECK_EQ(data->backing_store_[index++], variable_count);

    for (Variable* local : scope->locals_) {
      if (local->mode() == VAR || local->mode() == LET ||
          local->mode() == CONST) {
#ifdef DEBUG
        const AstRawString* local_name = local->raw_name();
        int name_length = data->backing_store_[index++];
        CHECK_EQ(name_length, local_name->length());
        for (int i = 0; i < name_length; ++i) {
          CHECK_EQ(data->backing_store_[index++], local_name->raw_data()[i]);
        }
#endif
        CHECK_EQ(data->backing_store_[index++], local->location());
        if (precise_maybe_assigned) {
          CHECK_EQ(data->backing_store_[index++], local->maybe_assigned());
        } else {
          STATIC_ASSERT(kMaybeAssigned > kNotAssigned);
          CHECK_GE(data->backing_store_[index++], local->maybe_assigned());
        }
      }
    }

    for (Scope* inner = scope->inner_scope(); inner != nullptr;
         inner = inner->sibling()) {
      if (PreParsedScopeData::HasVariablesWhichNeedAllocationData(inner)) {
        CompareScopeToData(inner, data, index, precise_maybe_assigned);
      }
    }
  }
};
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_SCOPE_TEST_HELPER_H_
