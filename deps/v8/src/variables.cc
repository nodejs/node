// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
    case CONST: return "CONST";
    case LET: return "LET";
    case CONST_HARMONY: return "CONST_HARMONY";
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
                   bool is_valid_LHS,
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
    is_valid_LHS_(is_valid_LHS),
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
