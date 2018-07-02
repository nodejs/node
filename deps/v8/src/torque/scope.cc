// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>
#include <string>

#include "src/torque/global-context.h"

#include "src/torque/scope.h"

namespace v8 {
namespace internal {
namespace torque {

Scope::Scope(ScopeChain& scope_chain)
    : scope_chain_(scope_chain),
      scope_number_(scope_chain_.GetNextScopeNumber()),
      private_label_number_(0) {}

Scope* ScopeChain::NewScope() {
  Scope* new_scope = new Scope(*this);
  scopes_.emplace_back(std::unique_ptr<Scope>(new_scope));
  return new_scope;
}

void Scope::AddLiveVariables(std::set<const Variable*>& set) {
  for (auto& current : lookup_) {
    if (current.second->IsVariable()) {
      set.insert(Variable::cast(current.second));
    }
  }
}

void Scope::Print() {
  std::cout << "scope #" << std::to_string(scope_number_) << "\n";
  for (auto& i : lookup_) {
    std::cout << i.first << ": " << i.second << "\n";
  }
}

Scope::Activator::Activator(Scope* scope) : scope_(scope) {
  scope->GetScopeChain().PushScope(scope);
}

Scope::Activator::~Activator() { scope_->GetScopeChain().PopScope(); }

}  // namespace torque
}  // namespace internal
}  // namespace v8
