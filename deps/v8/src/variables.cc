// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#include "ast.h"
#include "scopes.h"
#include "variables.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Implementation Variable.

const char* Variable::Mode2String(VariableMode mode) {
  switch (mode) {
    case VAR: return "VAR";
    case CONST_LEGACY: return "CONST_LEGACY";
    case LET: return "LET";
    case CONST: return "CONST";
    case MODULE: return "MODULE";
    case DYNAMIC: return "DYNAMIC";
    case DYNAMIC_GLOBAL: return "DYNAMIC_GLOBAL";
    case DYNAMIC_LOCAL: return "DYNAMIC_LOCAL";
    case INTERNAL: return "INTERNAL";
    case TEMPORARY: return "TEMPORARY";
  }
  UNREACHABLE();
  return NULL;
}


Variable::Variable(Scope* scope,
                   Handle<String> name,
                   VariableMode mode,
                   bool is_valid_ref,
                   Kind kind,
                   InitializationFlag initialization_flag,
                   Interface* interface)
  : scope_(scope),
    name_(name),
    mode_(mode),
    kind_(kind),
    location_(UNALLOCATED),
    index_(-1),
    initializer_position_(RelocInfo::kNoPosition),
    local_if_not_shadowed_(NULL),
    is_valid_ref_(is_valid_ref),
    force_context_allocation_(false),
    is_used_(false),
    initialization_flag_(initialization_flag),
    interface_(interface) {
  // Names must be canonicalized for fast equality checks.
  ASSERT(name->IsInternalizedString());
  // Var declared variables never need initialization.
  ASSERT(!(mode == VAR && initialization_flag == kNeedsInitialization));
}


bool Variable::IsGlobalObjectProperty() const {
  // Temporaries are never global, they must always be allocated in the
  // activation frame.
  return (IsDynamicVariableMode(mode_) ||
          (IsDeclaredVariableMode(mode_) && !IsLexicalVariableMode(mode_)))
      && scope_ != NULL && scope_->is_global_scope();
}


int Variable::CompareIndex(Variable* const* v, Variable* const* w) {
  int x = (*v)->index();
  int y = (*w)->index();
  // Consider sorting them according to type as well?
  return x - y;
}

} }  // namespace v8::internal
