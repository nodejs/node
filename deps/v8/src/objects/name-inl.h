// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NAME_INL_H_
#define V8_OBJECTS_NAME_INL_H_

#include "src/objects/name.h"

#include "src/heap/heap-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(Name)
CAST_ACCESSOR(Symbol)

ACCESSORS(Symbol, name, Object, kNameOffset)
SMI_ACCESSORS(Symbol, flags, kFlagsOffset)
BOOL_ACCESSORS(Symbol, flags, is_private, kPrivateBit)
BOOL_ACCESSORS(Symbol, flags, is_well_known_symbol, kWellKnownSymbolBit)
BOOL_ACCESSORS(Symbol, flags, is_public, kPublicBit)
BOOL_ACCESSORS(Symbol, flags, is_interesting_symbol, kInterestingSymbolBit)

TYPE_CHECKER(Symbol, SYMBOL_TYPE)

bool Name::IsUniqueName() const {
  uint32_t type = map()->instance_type();
  return (type & (kIsNotStringMask | kIsNotInternalizedMask)) !=
         (kStringTag | kNotInternalizedTag);
}

uint32_t Name::hash_field() {
  return READ_UINT32_FIELD(this, kHashFieldOffset);
}

void Name::set_hash_field(uint32_t value) {
  WRITE_UINT32_FIELD(this, kHashFieldOffset, value);
#if V8_HOST_ARCH_64_BIT
#if V8_TARGET_LITTLE_ENDIAN
  WRITE_UINT32_FIELD(this, kHashFieldSlot + kInt32Size, 0);
#else
  WRITE_UINT32_FIELD(this, kHashFieldSlot, 0);
#endif
#endif
}

bool Name::Equals(Name* other) {
  if (other == this) return true;
  if ((this->IsInternalizedString() && other->IsInternalizedString()) ||
      this->IsSymbol() || other->IsSymbol()) {
    return false;
  }
  return String::cast(this)->SlowEquals(String::cast(other));
}

bool Name::Equals(Handle<Name> one, Handle<Name> two) {
  if (one.is_identical_to(two)) return true;
  if ((one->IsInternalizedString() && two->IsInternalizedString()) ||
      one->IsSymbol() || two->IsSymbol()) {
    return false;
  }
  return String::SlowEquals(Handle<String>::cast(one),
                            Handle<String>::cast(two));
}

bool Name::IsHashFieldComputed(uint32_t field) {
  return (field & kHashNotComputedMask) == 0;
}

bool Name::HasHashCode() { return IsHashFieldComputed(hash_field()); }

uint32_t Name::Hash() {
  // Fast case: has hash code already been computed?
  uint32_t field = hash_field();
  if (IsHashFieldComputed(field)) return field >> kHashShift;
  // Slow case: compute hash code and set it. Has to be a string.
  return String::cast(this)->ComputeAndSetHash();
}

bool Name::IsInterestingSymbol() const {
  return IsSymbol() && Symbol::cast(this)->is_interesting_symbol();
}

bool Name::IsPrivate() {
  return this->IsSymbol() && Symbol::cast(this)->is_private();
}

bool Name::AsArrayIndex(uint32_t* index) {
  return IsString() && String::cast(this)->AsArrayIndex(index);
}

// static
bool Name::ContainsCachedArrayIndex(uint32_t hash) {
  return (hash & Name::kDoesNotContainCachedArrayIndexMask) == 0;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_NAME_INL_H_
