// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_SCOPES_INL_H_
#define V8_AST_SCOPES_INL_H_

#include "src/ast/scopes.h"

namespace v8 {
namespace internal {

template <typename T>
void Scope::ResolveScopesThenForEachVariable(DeclarationScope* max_outer_scope,
                                             T variable_proxy_stackvisitor,
                                             ParseInfo* info) {
  // Module variables must be allocated before variable resolution
  // to ensure that UpdateNeedsHoleCheck() can detect import variables.
  if (info != nullptr && is_module_scope()) {
    AsModuleScope()->AllocateModuleVariables();
  }
  // Lazy parsed declaration scopes are already partially analyzed. If there are
  // unresolved references remaining, they just need to be resolved in outer
  // scopes.
  Scope* lookup =
      is_declaration_scope() && AsDeclarationScope()->was_lazily_parsed()
          ? outer_scope()
          : this;

  for (VariableProxy *proxy = unresolved_list_.first(), *next = nullptr;
       proxy != nullptr; proxy = next) {
    next = proxy->next_unresolved();

    DCHECK(!proxy->is_resolved());
    Variable* var =
        lookup->LookupRecursive(info, proxy, max_outer_scope->outer_scope());
    if (var == nullptr) {
      variable_proxy_stackvisitor(proxy);
    } else if (var != Scope::kDummyPreParserVariable &&
               var != Scope::kDummyPreParserLexicalVariable) {
      if (info != nullptr) {
        // In this case we need to leave scopes in a way that they can be
        // allocated. If we resolved variables from lazy parsed scopes, we need
        // to context allocate the var.
        ResolveTo(info, proxy, var);
        if (!var->is_dynamic() && lookup != this) var->ForceContextAllocation();
      } else {
        var->set_is_used();
        if (proxy->is_assigned()) var->set_maybe_assigned();
      }
    }
  }

  // Clear unresolved_list_ as it's in an inconsistent state.
  unresolved_list_.Clear();

  for (Scope* scope = inner_scope_; scope != nullptr; scope = scope->sibling_) {
    scope->ResolveScopesThenForEachVariable(max_outer_scope,
                                            variable_proxy_stackvisitor, info);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_SCOPES_INL_H_
