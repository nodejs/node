// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/variables.h"

#include "src/ast/scopes.h"
#include "src/globals.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Implementation Variable.

Variable::Variable(Scope* scope, const AstRawString* name, VariableMode mode,
                   VariableKind kind, InitializationFlag initialization_flag,
                   MaybeAssignedFlag maybe_assigned_flag)
    : scope_(scope),
      name_(name),
      local_if_not_shadowed_(nullptr),
      next_(nullptr),
      index_(-1),
      initializer_position_(kNoSourcePosition),
      bit_field_(MaybeAssignedFlagField::encode(maybe_assigned_flag) |
                 InitializationFlagField::encode(initialization_flag) |
                 VariableModeField::encode(mode) | IsUsedField::encode(false) |
                 ForceContextAllocationField::encode(false) |
                 LocationField::encode(VariableLocation::UNALLOCATED) |
                 VariableKindField::encode(kind)) {
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

}  // namespace internal
}  // namespace v8
