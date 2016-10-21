// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/variables.h"

#include "src/ast/scopes.h"
#include "src/globals.h"

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
    case DYNAMIC: return "DYNAMIC";
    case DYNAMIC_GLOBAL: return "DYNAMIC_GLOBAL";
    case DYNAMIC_LOCAL: return "DYNAMIC_LOCAL";
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
      local_if_not_shadowed_(nullptr),
      index_(-1),
      initializer_position_(kNoSourcePosition),
      bit_field_(MaybeAssignedFlagField::encode(maybe_assigned_flag) |
                 InitializationFlagField::encode(initialization_flag) |
                 VariableModeField::encode(mode) | IsUsedField::encode(false) |
                 ForceContextAllocationField::encode(false) |
                 LocationField::encode(VariableLocation::UNALLOCATED) |
                 KindField::encode(kind)) {
  // Var declared variables never need initialization.
  DCHECK(!(mode == VAR && initialization_flag == kNeedsInitialization));
}


bool Variable::IsGlobalObjectProperty() const {
  // Temporaries are never global, they must always be allocated in the
  // activation frame.
  return (IsDynamicVariableMode(mode()) ||
          (IsDeclaredVariableMode(mode()) && !IsLexicalVariableMode(mode()))) &&
         scope_ != NULL && scope_->is_script_scope();
}


bool Variable::IsStaticGlobalObjectProperty() const {
  // Temporaries are never global, they must always be allocated in the
  // activation frame.
  return (IsDeclaredVariableMode(mode()) && !IsLexicalVariableMode(mode())) &&
         scope_ != NULL && scope_->is_script_scope();
}


int Variable::CompareIndex(Variable* const* v, Variable* const* w) {
  int x = (*v)->index();
  int y = (*w)->index();
  // Consider sorting them according to type as well?
  return x - y;
}

}  // namespace internal
}  // namespace v8
