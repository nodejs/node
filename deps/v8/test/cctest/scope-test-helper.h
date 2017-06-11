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

  static void AllocateWithoutVariableResolution(Scope* scope) {
    scope->AllocateVariablesRecursively();
  }

  static void CompareScopes(Scope* baseline, Scope* scope,
                            bool precise_maybe_assigned) {
    if (!scope->is_hidden()) {
      for (auto baseline_local = baseline->locals()->begin(),
                scope_local = scope->locals()->begin();
           baseline_local != baseline->locals()->end();
           ++baseline_local, ++scope_local) {
        if (scope_local->mode() == VAR || scope_local->mode() == LET ||
            scope_local->mode() == CONST) {
          // Sanity check the variable name. If this fails, the variable order
          // is not deterministic.
          CHECK_EQ(scope_local->raw_name()->length(),
                   baseline_local->raw_name()->length());
          for (int i = 0; i < scope_local->raw_name()->length(); ++i) {
            CHECK_EQ(scope_local->raw_name()->raw_data()[i],
                     baseline_local->raw_name()->raw_data()[i]);
          }

          CHECK_EQ(scope_local->location(), baseline_local->location());
          if (precise_maybe_assigned) {
            CHECK_EQ(scope_local->maybe_assigned(),
                     baseline_local->maybe_assigned());
          } else {
            STATIC_ASSERT(kMaybeAssigned > kNotAssigned);
            CHECK_GE(scope_local->maybe_assigned(),
                     baseline_local->maybe_assigned());
          }
        }
      }
    }

    for (Scope *baseline_inner = baseline->inner_scope(),
               *scope_inner = scope->inner_scope();
         scope_inner != nullptr; scope_inner = scope_inner->sibling(),
               baseline_inner = baseline_inner->sibling()) {
      CompareScopes(baseline_inner, scope_inner, precise_maybe_assigned);
    }
  }
};
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_SCOPE_TEST_HELPER_H_
