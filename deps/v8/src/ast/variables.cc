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
  return (IsDynamicVariableMode(mode()) || mode() == VAR) &&
         scope_ != nullptr && scope_->is_script_scope();
}

}  // namespace internal
}  // namespace v8
