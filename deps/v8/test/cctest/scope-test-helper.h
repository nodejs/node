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

  static void CompareScopes(Scope* baseline, Scope* scope,
                            bool precise_maybe_assigned) {
    CHECK_EQ(baseline->scope_type(), scope->scope_type());
    CHECK_IMPLIES(baseline->is_declaration_scope(),
                  baseline->AsDeclarationScope()->function_kind() ==
                      scope->AsDeclarationScope()->function_kind());

    if (!PreParsedScopeDataBuilder::ScopeNeedsData(baseline)) {
      return;
    }

    if (scope->is_declaration_scope() &&
        scope->AsDeclarationScope()->is_skipped_function()) {
      return;
    }

    if (baseline->scope_type() == ScopeType::FUNCTION_SCOPE) {
      Variable* function = baseline->AsDeclarationScope()->function_var();
      if (function != nullptr) {
        CompareVariables(function, scope->AsDeclarationScope()->function_var(),
                         precise_maybe_assigned);
      } else {
        CHECK_NULL(scope->AsDeclarationScope()->function_var());
      }
    }

    for (auto baseline_local = baseline->locals()->begin(),
              scope_local = scope->locals()->begin();
         baseline_local != baseline->locals()->end();
         ++baseline_local, ++scope_local) {
      if (scope_local->mode() == VariableMode::kVar ||
          scope_local->mode() == VariableMode::kLet ||
          scope_local->mode() == VariableMode::kConst) {
        CompareVariables(*baseline_local, *scope_local, precise_maybe_assigned);
      }
    }

    for (Scope *baseline_inner = baseline->inner_scope(),
               *scope_inner = scope->inner_scope();
         scope_inner != nullptr; scope_inner = scope_inner->sibling(),
               baseline_inner = baseline_inner->sibling()) {
      CompareScopes(baseline_inner, scope_inner, precise_maybe_assigned);
    }
  }

  static void CompareVariables(Variable* baseline_local, Variable* scope_local,
                               bool precise_maybe_assigned) {
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
      CHECK_EQ(scope_local->maybe_assigned(), baseline_local->maybe_assigned());
    } else {
      STATIC_ASSERT(kMaybeAssigned > kNotAssigned);
      CHECK_GE(scope_local->maybe_assigned(), baseline_local->maybe_assigned());
    }
  }

  // Finds a scope given a start point and directions to it (which inner scope
  // to pick).
  static Scope* FindScope(Scope* scope, const std::vector<unsigned>& location) {
    for (auto n : location) {
      scope = scope->inner_scope();
      CHECK_NOT_NULL(scope);
      while (n-- > 0) {
        scope = scope->sibling();
        CHECK_NOT_NULL(scope);
      }
    }
    return scope;
  }

  static void MarkInnerFunctionsAsSkipped(Scope* scope) {
    for (Scope* inner = scope->inner_scope(); inner != nullptr;
         inner = inner->sibling()) {
      if (inner->scope_type() == ScopeType::FUNCTION_SCOPE &&
          !inner->AsDeclarationScope()->is_arrow_scope()) {
        inner->AsDeclarationScope()->set_is_skipped_function(true);
      }
      MarkInnerFunctionsAsSkipped(inner);
    }
  }

  static bool HasSkippedFunctionInside(Scope* scope) {
    if (scope->scope_type() == ScopeType::FUNCTION_SCOPE &&
        scope->AsDeclarationScope()->is_skipped_function()) {
      return true;
    }
    for (Scope* inner = scope->inner_scope(); inner != nullptr;
         inner = inner->sibling()) {
      if (HasSkippedFunctionInside(inner)) {
        return true;
      }
    }
    return false;
  }
};
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_SCOPE_TEST_HELPER_H_
