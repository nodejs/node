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

#ifndef V8_VARIABLES_H_
#define V8_VARIABLES_H_

#include "zone.h"

namespace v8 {
namespace internal {

// Variables and AST expression nodes can track their "type" to enable
// optimizations and removal of redundant checks when generating code.

class StaticType {
 public:
  enum Kind {
    UNKNOWN,
    LIKELY_SMI
  };

  StaticType() : kind_(UNKNOWN) {}

  bool Is(Kind kind) const { return kind_ == kind; }

  bool IsKnown() const { return !Is(UNKNOWN); }
  bool IsUnknown() const { return Is(UNKNOWN); }
  bool IsLikelySmi() const { return Is(LIKELY_SMI); }

  void CopyFrom(StaticType* other) {
    kind_ = other->kind_;
  }

  static const char* Type2String(StaticType* type);

  // LIKELY_SMI accessors
  void SetAsLikelySmi() {
    kind_ = LIKELY_SMI;
  }

  void SetAsLikelySmiIfUnknown() {
    if (IsUnknown()) {
      SetAsLikelySmi();
    }
  }

 private:
  Kind kind_;
};


// The AST refers to variables via VariableProxies - placeholders for the actual
// variables. Variables themselves are never directly referred to from the AST,
// they are maintained by scopes, and referred to from VariableProxies and Slots
// after binding and variable allocation.

class Variable: public ZoneObject {
 public:
  enum Mode {
    // User declared variables:
    VAR,       // declared via 'var', and 'function' declarations

    CONST,     // declared via 'const' declarations

    // Variables introduced by the compiler:
    DYNAMIC,         // always require dynamic lookup (we don't know
                     // the declaration)

    DYNAMIC_GLOBAL,  // requires dynamic lookup, but we know that the
                     // variable is global unless it has been shadowed
                     // by an eval-introduced variable

    DYNAMIC_LOCAL,   // requires dynamic lookup, but we know that the
                     // variable is local and where it is unless it
                     // has been shadowed by an eval-introduced
                     // variable

    INTERNAL,        // like VAR, but not user-visible (may or may not
                     // be in a context)

    TEMPORARY        // temporary variables (not user-visible), never
                     // in a context
  };

  enum Kind {
    NORMAL,
    THIS,
    ARGUMENTS
  };

  Variable(Scope* scope,
           Handle<String> name,
           Mode mode,
           bool is_valid_lhs,
           Kind kind);

  // Printing support
  static const char* Mode2String(Mode mode);

  // Type testing & conversion
  Property* AsProperty() const;
  Slot* AsSlot() const;

  bool IsValidLeftHandSide() { return is_valid_LHS_; }

  // The source code for an eval() call may refer to a variable that is
  // in an outer scope about which we don't know anything (it may not
  // be the global scope). scope() is NULL in that case. Currently the
  // scope is only used to follow the context chain length.
  Scope* scope() const { return scope_; }

  Handle<String> name() const { return name_; }
  Mode mode() const { return mode_; }
  bool is_accessed_from_inner_scope() const {
    return is_accessed_from_inner_scope_;
  }
  void MarkAsAccessedFromInnerScope() {
    is_accessed_from_inner_scope_ = true;
  }
  bool is_used() { return is_used_; }
  void set_is_used(bool flag) { is_used_ = flag; }

  bool IsVariable(Handle<String> n) const {
    return !is_this() && name().is_identical_to(n);
  }

  bool IsStackAllocated() const;
  bool IsParameter() const;  // Includes 'this'.
  bool IsStackLocal() const;
  bool IsContextSlot() const;

  bool is_dynamic() const {
    return (mode_ == DYNAMIC ||
            mode_ == DYNAMIC_GLOBAL ||
            mode_ == DYNAMIC_LOCAL);
  }

  bool is_global() const;
  bool is_this() const { return kind_ == THIS; }
  bool is_arguments() const { return kind_ == ARGUMENTS; }

  // True if the variable is named eval and not known to be shadowed.
  bool is_possibly_eval() const {
    return IsVariable(Factory::eval_symbol()) &&
        (mode_ == DYNAMIC || mode_ == DYNAMIC_GLOBAL);
  }

  Variable* local_if_not_shadowed() const {
    ASSERT(mode_ == DYNAMIC_LOCAL && local_if_not_shadowed_ != NULL);
    return local_if_not_shadowed_;
  }

  void set_local_if_not_shadowed(Variable* local) {
    local_if_not_shadowed_ = local;
  }

  Expression* rewrite() const { return rewrite_; }
  void set_rewrite(Expression* expr) { rewrite_ = expr; }

  StaticType* type() { return &type_; }

 private:
  Scope* scope_;
  Handle<String> name_;
  Mode mode_;
  bool is_valid_LHS_;
  Kind kind_;

  Variable* local_if_not_shadowed_;

  // Usage info.
  bool is_accessed_from_inner_scope_;  // set by variable resolver
  bool is_used_;

  // Static type information
  StaticType type_;

  // Code generation.
  // rewrite_ is usually a Slot or a Property, but may be any expression.
  Expression* rewrite_;
};


} }  // namespace v8::internal

#endif  // V8_VARIABLES_H_
