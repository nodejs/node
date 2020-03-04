// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/variables.h"

#include "src/ast/scopes.h"
#include "src/common/globals.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Implementation Variable.

Variable::Variable(Variable* other)
    : scope_(other->scope_),
      name_(other->name_),
      local_if_not_shadowed_(nullptr),
      next_(nullptr),
      index_(other->index_),
      initializer_position_(other->initializer_position_),
      bit_field_(other->bit_field_) {}

bool Variable::IsGlobalObjectProperty() const {
  // Temporaries are never global, they must always be allocated in the
  // activation frame.
  return (IsDynamicVariableMode(mode()) || mode() == VariableMode::kVar) &&
         scope_ != nullptr && scope_->is_script_scope();
}

bool Variable::IsReplGlobalLet() const {
  DCHECK_IMPLIES(scope()->is_repl_mode_scope(), scope()->is_script_scope());
  return scope()->is_repl_mode_scope() && mode() == VariableMode::kLet;
}

void Variable::RewriteLocationForRepl() {
  DCHECK(scope_->is_repl_mode_scope());

  if (mode() == VariableMode::kLet) {
    DCHECK_EQ(location(), VariableLocation::CONTEXT);
    bit_field_ =
        LocationField::update(bit_field_, VariableLocation::REPL_GLOBAL);
  }
}

}  // namespace internal
}  // namespace v8
