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
// Implementation UseCount.

UseCount::UseCount()
  : nreads_(0),
    nwrites_(0) {
}


void UseCount::RecordRead(int weight) {
  ASSERT(weight > 0);
  nreads_ += weight;
  // We must have a positive nreads_ here. Handle
  // any kind of overflow by setting nreads_ to
  // some large-ish value.
  if (nreads_ <= 0) nreads_ = 1000000;
  ASSERT(is_read() & is_used());
}


void UseCount::RecordWrite(int weight) {
  ASSERT(weight > 0);
  nwrites_ += weight;
  // We must have a positive nwrites_ here. Handle
  // any kind of overflow by setting nwrites_ to
  // some large-ish value.
  if (nwrites_ <= 0) nwrites_ = 1000000;
  ASSERT(is_written() && is_used());
}


void UseCount::RecordAccess(int weight) {
  RecordRead(weight);
  RecordWrite(weight);
}


void UseCount::RecordUses(UseCount* uses) {
  if (uses->nreads() > 0) RecordRead(uses->nreads());
  if (uses->nwrites() > 0) RecordWrite(uses->nwrites());
}


#ifdef DEBUG
void UseCount::Print() {
  // PrintF("r = %d, w = %d", nreads_, nwrites_);
  PrintF("%du = %dr + %dw", nuses(), nreads(), nwrites());
}
#endif


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


Property* Variable::AsProperty() {
  return rewrite_ == NULL ? NULL : rewrite_->AsProperty();
}


Variable* Variable::AsVariable()  {
  return rewrite_ == NULL || rewrite_->AsSlot() != NULL ? this : NULL;
}


Slot* Variable::slot() const {
  return rewrite_ != NULL ? rewrite_->AsSlot() : NULL;
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
