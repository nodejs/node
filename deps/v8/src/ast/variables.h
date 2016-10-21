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
class Variable final : public ZoneObject {
 public:
  enum Kind : uint8_t {
    NORMAL,
    FUNCTION,
    THIS,
    ARGUMENTS,
    kLastKind = ARGUMENTS
  };

  Variable(Scope* scope, const AstRawString* name, VariableMode mode, Kind kind,
           InitializationFlag initialization_flag,
           MaybeAssignedFlag maybe_assigned_flag = kNotAssigned);

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
  VariableMode mode() const { return VariableModeField::decode(bit_field_); }
  bool has_forced_context_allocation() const {
    return ForceContextAllocationField::decode(bit_field_);
  }
  void ForceContextAllocation() {
    bit_field_ = ForceContextAllocationField::update(bit_field_, true);
  }
  bool is_used() { return IsUsedField::decode(bit_field_); }
  void set_is_used() { bit_field_ = IsUsedField::update(bit_field_, true); }
  MaybeAssignedFlag maybe_assigned() const {
    return MaybeAssignedFlagField::decode(bit_field_);
  }
  void set_maybe_assigned() {
    bit_field_ = MaybeAssignedFlagField::update(bit_field_, kMaybeAssigned);
  }

  int initializer_position() { return initializer_position_; }
  void set_initializer_position(int pos) { initializer_position_ = pos; }

  bool IsUnallocated() const {
    return location() == VariableLocation::UNALLOCATED;
  }
  bool IsParameter() const { return location() == VariableLocation::PARAMETER; }
  bool IsStackLocal() const { return location() == VariableLocation::LOCAL; }
  bool IsStackAllocated() const { return IsParameter() || IsStackLocal(); }
  bool IsContextSlot() const { return location() == VariableLocation::CONTEXT; }
  bool IsGlobalSlot() const { return location() == VariableLocation::GLOBAL; }
  bool IsUnallocatedOrGlobalSlot() const {
    return IsUnallocated() || IsGlobalSlot();
  }
  bool IsLookupSlot() const { return location() == VariableLocation::LOOKUP; }
  bool IsGlobalObjectProperty() const;
  bool IsStaticGlobalObjectProperty() const;

  bool is_dynamic() const { return IsDynamicVariableMode(mode()); }
  bool is_const_mode() const { return IsImmutableVariableMode(mode()); }
  bool binding_needs_init() const {
    DCHECK(initialization_flag() != kNeedsInitialization ||
           IsLexicalVariableMode(mode()));
    return initialization_flag() == kNeedsInitialization;
  }

  bool is_function() const { return kind() == FUNCTION; }
  bool is_this() const { return kind() == THIS; }
  bool is_arguments() const { return kind() == ARGUMENTS; }

  Variable* local_if_not_shadowed() const {
    DCHECK(mode() == DYNAMIC_LOCAL && local_if_not_shadowed_ != NULL);
    return local_if_not_shadowed_;
  }

  void set_local_if_not_shadowed(Variable* local) {
    local_if_not_shadowed_ = local;
  }

  VariableLocation location() const {
    return LocationField::decode(bit_field_);
  }
  Kind kind() const { return KindField::decode(bit_field_); }
  InitializationFlag initialization_flag() const {
    return InitializationFlagField::decode(bit_field_);
  }

  int index() const { return index_; }

  void AllocateTo(VariableLocation location, int index) {
    DCHECK(IsUnallocated() ||
           (this->location() == location && this->index() == index));
    bit_field_ = LocationField::update(bit_field_, location);
    DCHECK_EQ(location, this->location());
    index_ = index;
  }

  static int CompareIndex(Variable* const* v, Variable* const* w);

 private:
  Scope* scope_;
  const AstRawString* name_;

  // If this field is set, this variable references the stored locally bound
  // variable, but it might be shadowed by variable bindings introduced by
  // sloppy 'eval' calls between the reference scope (inclusive) and the
  // binding scope (exclusive).
  Variable* local_if_not_shadowed_;
  int index_;
  int initializer_position_;
  uint16_t bit_field_;

  class VariableModeField : public BitField16<VariableMode, 0, 3> {};
  class KindField : public BitField16<Kind, VariableModeField::kNext, 2> {};
  class LocationField
      : public BitField16<VariableLocation, KindField::kNext, 3> {};
  class ForceContextAllocationField
      : public BitField16<bool, LocationField::kNext, 1> {};
  class IsUsedField
      : public BitField16<bool, ForceContextAllocationField::kNext, 1> {};
  class InitializationFlagField
      : public BitField16<InitializationFlag, IsUsedField::kNext, 2> {};
  class MaybeAssignedFlagField
      : public BitField16<MaybeAssignedFlag, InitializationFlagField::kNext,
                          2> {};
};
}  // namespace internal
}  // namespace v8

#endif  // V8_AST_VARIABLES_H_
