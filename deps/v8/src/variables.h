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

#ifndef V8_VARIABLES_H_
#define V8_VARIABLES_H_

#include "zone.h"
#include "interface.h"

namespace v8 {
namespace internal {

// The AST refers to variables via VariableProxies - placeholders for the actual
// variables. Variables themselves are never directly referred to from the AST,
// they are maintained by scopes, and referred to from VariableProxies and Slots
// after binding and variable allocation.

class Variable: public ZoneObject {
 public:
  enum Kind {
    NORMAL,
    THIS,
    ARGUMENTS
  };

  enum Location {
    // Before and during variable allocation, a variable whose location is
    // not yet determined.  After allocation, a variable looked up as a
    // property on the global object (and possibly absent).  name() is the
    // variable name, index() is invalid.
    UNALLOCATED,

    // A slot in the parameter section on the stack.  index() is the
    // parameter index, counting left-to-right.  The receiver is index -1;
    // the first parameter is index 0.
    PARAMETER,

    // A slot in the local section on the stack.  index() is the variable
    // index in the stack frame, starting at 0.
    LOCAL,

    // An indexed slot in a heap context.  index() is the variable index in
    // the context object on the heap, starting at 0.  scope() is the
    // corresponding scope.
    CONTEXT,

    // A named slot in a heap context.  name() is the variable name in the
    // context object on the heap, with lookup starting at the current
    // context.  index() is invalid.
    LOOKUP
  };

  Variable(Scope* scope,
           Handle<String> name,
           VariableMode mode,
           bool is_valid_lhs,
           Kind kind,
           InitializationFlag initialization_flag,
           Interface* interface = Interface::NewValue());

  // Printing support
  static const char* Mode2String(VariableMode mode);

  bool IsValidLeftHandSide() { return is_valid_LHS_; }

  // The source code for an eval() call may refer to a variable that is
  // in an outer scope about which we don't know anything (it may not
  // be the global scope). scope() is NULL in that case. Currently the
  // scope is only used to follow the context chain length.
  Scope* scope() const { return scope_; }

  Handle<String> name() const { return name_; }
  VariableMode mode() const { return mode_; }
  bool has_forced_context_allocation() const {
    return force_context_allocation_;
  }
  void ForceContextAllocation() {
    ASSERT(mode_ != TEMPORARY);
    force_context_allocation_ = true;
  }
  bool is_used() { return is_used_; }
  void set_is_used(bool flag) { is_used_ = flag; }

  int initializer_position() { return initializer_position_; }
  void set_initializer_position(int pos) { initializer_position_ = pos; }

  bool IsVariable(Handle<String> n) const {
    return !is_this() && name().is_identical_to(n);
  }

  bool IsUnallocated() const { return location_ == UNALLOCATED; }
  bool IsParameter() const { return location_ == PARAMETER; }
  bool IsStackLocal() const { return location_ == LOCAL; }
  bool IsStackAllocated() const { return IsParameter() || IsStackLocal(); }
  bool IsContextSlot() const { return location_ == CONTEXT; }
  bool IsLookupSlot() const { return location_ == LOOKUP; }
  bool IsGlobalObjectProperty() const;

  bool is_dynamic() const { return IsDynamicVariableMode(mode_); }
  bool is_const_mode() const { return IsImmutableVariableMode(mode_); }
  bool binding_needs_init() const {
    return initialization_flag_ == kNeedsInitialization;
  }

  bool is_this() const { return kind_ == THIS; }
  bool is_arguments() const { return kind_ == ARGUMENTS; }

  // True if the variable is named eval and not known to be shadowed.
  bool is_possibly_eval(Isolate* isolate) const {
    return IsVariable(isolate->factory()->eval_string());
  }

  Variable* local_if_not_shadowed() const {
    ASSERT(mode_ == DYNAMIC_LOCAL && local_if_not_shadowed_ != NULL);
    return local_if_not_shadowed_;
  }

  void set_local_if_not_shadowed(Variable* local) {
    local_if_not_shadowed_ = local;
  }

  Location location() const { return location_; }
  int index() const { return index_; }
  InitializationFlag initialization_flag() const {
    return initialization_flag_;
  }
  Interface* interface() const { return interface_; }

  void AllocateTo(Location location, int index) {
    location_ = location;
    index_ = index;
  }

  static int CompareIndex(Variable* const* v, Variable* const* w);

 private:
  Scope* scope_;
  Handle<String> name_;
  VariableMode mode_;
  Kind kind_;
  Location location_;
  int index_;
  int initializer_position_;

  // If this field is set, this variable references the stored locally bound
  // variable, but it might be shadowed by variable bindings introduced by
  // sloppy 'eval' calls between the reference scope (inclusive) and the
  // binding scope (exclusive).
  Variable* local_if_not_shadowed_;

  // Valid as a LHS? (const and this are not valid LHS, for example)
  bool is_valid_LHS_;

  // Usage info.
  bool force_context_allocation_;  // set by variable resolver
  bool is_used_;
  InitializationFlag initialization_flag_;

  // Module type info.
  Interface* interface_;
};


} }  // namespace v8::internal

#endif  // V8_VARIABLES_H_
