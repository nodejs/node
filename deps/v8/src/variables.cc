// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ast.h"
#include "src/scopes.h"
#include "src/variables.h"

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
    case IMPORT: return "IMPORT";
    case DYNAMIC: return "DYNAMIC";
    case DYNAMIC_GLOBAL: return "DYNAMIC_GLOBAL";
    case DYNAMIC_LOCAL: return "DYNAMIC_LOCAL";
    case INTERNAL: return "INTERNAL";
    case TEMPORARY: return "TEMPORARY";
  }
  UNREACHABLE();
  return NULL;
}


Variable::Variable(Scope* scope, const AstRawString* name, VariableMode mode,
                   Kind kind, InitializationFlag initialization_flag,
                   MaybeAssignedFlag maybe_assigned_flag)
    : scope_(scope),
      name_(name),
      mode_(mode),
      kind_(kind),
      location_(UNALLOCATED),
      index_(-1),
      initializer_position_(RelocInfo::kNoPosition),
      has_strong_mode_reference_(false),
      strong_mode_reference_start_position_(RelocInfo::kNoPosition),
      strong_mode_reference_end_position_(RelocInfo::kNoPosition),
      local_if_not_shadowed_(NULL),
      force_context_allocation_(false),
      is_used_(false),
      initialization_flag_(initialization_flag),
      maybe_assigned_(maybe_assigned_flag) {
  // Var declared variables never need initialization.
  DCHECK(!(mode == VAR && initialization_flag == kNeedsInitialization));
}


bool Variable::IsGlobalObjectProperty() const {
  // Temporaries are never global, they must always be allocated in the
  // activation frame.
  return (IsDynamicVariableMode(mode_) ||
          (IsDeclaredVariableMode(mode_) && !IsLexicalVariableMode(mode_))) &&
         scope_ != NULL && scope_->is_script_scope() && !is_this();
}


int Variable::CompareIndex(Variable* const* v, Variable* const* w) {
  int x = (*v)->index();
  int y = (*w)->index();
  // Consider sorting them according to type as well?
  return x - y;
}

}  // namespace internal
}  // namespace v8
