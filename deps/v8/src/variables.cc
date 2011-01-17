// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
// Implementation StaticType.


const char* StaticType::Type2String(StaticType* type) {
  switch (type->kind_) {
    case UNKNOWN:
      return "UNKNOWN";
    case LIKELY_SMI:
      return "LIKELY_SMI";
    default:
      UNREACHABLE();
  }
  return "UNREACHABLE";
}


// ----------------------------------------------------------------------------
// Implementation Variable.


const char* Variable::Mode2String(Mode mode) {
  switch (mode) {
    case VAR: return "VAR";
    case CONST: return "CONST";
    case DYNAMIC: return "DYNAMIC";
    case DYNAMIC_GLOBAL: return "DYNAMIC_GLOBAL";
    case DYNAMIC_LOCAL: return "DYNAMIC_LOCAL";
    case INTERNAL: return "INTERNAL";
    case TEMPORARY: return "TEMPORARY";
  }
  UNREACHABLE();
  return NULL;
}


Property* Variable::AsProperty() const {
  return rewrite_ == NULL ? NULL : rewrite_->AsProperty();
}


Slot* Variable::AsSlot() const {
  return rewrite_ == NULL ? NULL : rewrite_->AsSlot();
}


bool Variable::IsStackAllocated() const {
  Slot* slot = AsSlot();
  return slot != NULL && slot->IsStackAllocated();
}


bool Variable::IsParameter() const {
  Slot* s = AsSlot();
  return s != NULL && s->type() == Slot::PARAMETER;
}


bool Variable::IsStackLocal() const {
  Slot* s = AsSlot();
  return s != NULL && s->type() == Slot::LOCAL;
}


bool Variable::IsContextSlot() const {
  Slot* s = AsSlot();
  return s != NULL && s->type() == Slot::CONTEXT;
}


Variable::Variable(Scope* scope,
                   Handle<String> name,
                   Mode mode,
                   bool is_valid_LHS,
                   Kind kind)
  : scope_(scope),
    name_(name),
    mode_(mode),
    is_valid_LHS_(is_valid_LHS),
    kind_(kind),
    local_if_not_shadowed_(NULL),
    is_accessed_from_inner_scope_(false),
    is_used_(false),
    rewrite_(NULL) {
  // names must be canonicalized for fast equality checks
  ASSERT(name->IsSymbol());
}


bool Variable::is_global() const {
  // Temporaries are never global, they must always be allocated in the
  // activation frame.
  return mode_ != TEMPORARY && scope_ != NULL && scope_->is_global_scope();
}

} }  // namespace v8::internal
