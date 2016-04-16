// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_VARIABLES_H_
#define V8_AST_VARIABLES_H_

#include "src/ast/ast-value-factory.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// The AST refers to variables via VariableProxies - placeholders for the actual
// variables. Variables themselves are never directly referred to from the AST,
// they are maintained by scopes, and referred to from VariableProxies and Slots
// after binding and variable allocation.
class Variable: public ZoneObject {
 public:
  enum Kind { NORMAL, FUNCTION, THIS, ARGUMENTS };

  Variable(Scope* scope, const AstRawString* name, VariableMode mode, Kind kind,
           InitializationFlag initialization_flag,
           MaybeAssignedFlag maybe_assigned_flag = kNotAssigned);

  virtual ~Variable() {}

  // Printing support
  static const char* Mode2String(VariableMode mode);

  // The source code for an eval() call may refer to a variable that is
  // in an outer scope about which we don't know anything (it may not
  // be the script scope). scope() is NULL in that case. Currently the
  // scope is only used to follow the context chain length.
  Scope* scope() const { return scope_; }

  // This is for adjusting the scope of temporaries used when desugaring
  // parameter initializers.
  void set_scope(Scope* scope) { scope_ = scope; }

  Handle<String> name() const { return name_->string(); }
  const AstRawString* raw_name() const { return name_; }
  VariableMode mode() const { return mode_; }
  bool has_forced_context_allocation() const {
    return force_context_allocation_;
  }
  void ForceContextAllocation() {
    force_context_allocation_ = true;
  }
  bool is_used() { return is_used_; }
  void set_is_used() { is_used_ = true; }
  MaybeAssignedFlag maybe_assigned() const { return maybe_assigned_; }
  void set_maybe_assigned() { maybe_assigned_ = kMaybeAssigned; }

  int initializer_position() { return initializer_position_; }
  void set_initializer_position(int pos) { initializer_position_ = pos; }

  bool IsVariable(Handle<String> n) const {
    return !is_this() && name().is_identical_to(n);
  }

  bool IsUnallocated() const {
    return location_ == VariableLocation::UNALLOCATED;
  }
  bool IsParameter() const { return location_ == VariableLocation::PARAMETER; }
  bool IsStackLocal() const { return location_ == VariableLocation::LOCAL; }
  bool IsStackAllocated() const { return IsParameter() || IsStackLocal(); }
  bool IsContextSlot() const { return location_ == VariableLocation::CONTEXT; }
  bool IsGlobalSlot() const { return location_ == VariableLocation::GLOBAL; }
  bool IsUnallocatedOrGlobalSlot() const {
    return IsUnallocated() || IsGlobalSlot();
  }
  bool IsLookupSlot() const { return location_ == VariableLocation::LOOKUP; }
  bool IsGlobalObjectProperty() const;
  bool IsStaticGlobalObjectProperty() const;

  bool is_dynamic() const { return IsDynamicVariableMode(mode_); }
  bool is_const_mode() const { return IsImmutableVariableMode(mode_); }
  bool binding_needs_init() const {
    return initialization_flag_ == kNeedsInitialization;
  }

  bool is_function() const { return kind_ == FUNCTION; }
  bool is_this() const { return kind_ == THIS; }
  bool is_arguments() const { return kind_ == ARGUMENTS; }

  // For script scopes, the "this" binding is provided by a ScriptContext added
  // to the global's ScriptContextTable.  This binding might not statically
  // resolve to a Variable::THIS binding, instead being DYNAMIC_LOCAL.  However
  // any variable named "this" does indeed refer to a Variable::THIS binding;
  // the grammar ensures this to be the case.  So wherever a "this" binding
  // might be provided by the global, use HasThisName instead of is_this().
  bool HasThisName(Isolate* isolate) const {
    return is_this() || *name() == *isolate->factory()->this_string();
  }

  // True if the variable is named eval and not known to be shadowed.
  bool is_possibly_eval(Isolate* isolate) const {
    return IsVariable(isolate->factory()->eval_string());
  }

  Variable* local_if_not_shadowed() const {
    DCHECK(mode_ == DYNAMIC_LOCAL && local_if_not_shadowed_ != NULL);
    return local_if_not_shadowed_;
  }

  void set_local_if_not_shadowed(Variable* local) {
    local_if_not_shadowed_ = local;
  }

  VariableLocation location() const { return location_; }
  int index() const { return index_; }
  InitializationFlag initialization_flag() const {
    return initialization_flag_;
  }

  void AllocateTo(VariableLocation location, int index) {
    location_ = location;
    index_ = index;
  }

  void SetFromEval() { is_from_eval_ = true; }

  static int CompareIndex(Variable* const* v, Variable* const* w);

  PropertyAttributes DeclarationPropertyAttributes() const {
    int property_attributes = NONE;
    if (IsImmutableVariableMode(mode_)) {
      property_attributes |= READ_ONLY;
    }
    if (is_from_eval_) {
      property_attributes |= EVAL_DECLARED;
    }
    return static_cast<PropertyAttributes>(property_attributes);
  }

 private:
  Scope* scope_;
  const AstRawString* name_;
  VariableMode mode_;
  Kind kind_;
  VariableLocation location_;
  int index_;
  int initializer_position_;

  // If this field is set, this variable references the stored locally bound
  // variable, but it might be shadowed by variable bindings introduced by
  // sloppy 'eval' calls between the reference scope (inclusive) and the
  // binding scope (exclusive).
  Variable* local_if_not_shadowed_;

  // True if this variable is introduced by a sloppy eval
  bool is_from_eval_;

  // Usage info.
  bool force_context_allocation_;  // set by variable resolver
  bool is_used_;
  InitializationFlag initialization_flag_;
  MaybeAssignedFlag maybe_assigned_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_AST_VARIABLES_H_
