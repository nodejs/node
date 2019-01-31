// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NAME_INL_H_
#define V8_OBJECTS_NAME_INL_H_

#include "src/objects/name.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/objects/map-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Name, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(Symbol, Name)

CAST_ACCESSOR(Name)
CAST_ACCESSOR(Symbol)

ACCESSORS(Symbol, name, Object, kNameOffset)
INT_ACCESSORS(Symbol, flags, kFlagsOffset)
BIT_FIELD_ACCESSORS(Symbol, flags, is_private, Symbol::IsPrivateBit)
BIT_FIELD_ACCESSORS(Symbol, flags, is_well_known_symbol,
                    Symbol::IsWellKnownSymbolBit)
BIT_FIELD_ACCESSORS(Symbol, flags, is_public, Symbol::IsPublicBit)
BIT_FIELD_ACCESSORS(Symbol, flags, is_interesting_symbol,
                    Symbol::IsInterestingSymbolBit)

bool Symbol::is_private_name() const {
  bool value = Symbol::IsPrivateNameBit::decode(flags());
  DCHECK_IMPLIES(value, is_private());
  return value;
}

void Symbol::set_is_private_name() {
  // TODO(gsathya): Re-order the bits to have these next to each other
  // and just do the bit shifts once.
  set_flags(Symbol::IsPrivateBit::update(flags(), true));
  set_flags(Symbol::IsPrivateNameBit::update(flags(), true));
}

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
}

bool Name::Equals(Name other) {
  if (other == *this) return true;
  if ((this->IsInternalizedString() && other->IsInternalizedString()) ||
      this->IsSymbol() || other->IsSymbol()) {
    return false;
  }
  return String::cast(*this)->SlowEquals(String::cast(other));
}

bool Name::Equals(Isolate* isolate, Handle<Name> one, Handle<Name> two) {
  if (one.is_identical_to(two)) return true;
  if ((one->IsInternalizedString() && two->IsInternalizedString()) ||
      one->IsSymbol() || two->IsSymbol()) {
    return false;
  }
  return String::SlowEquals(isolate, Handle<String>::cast(one),
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
  // Also the string must be writable, because read-only strings will have their
  // hash values precomputed.
  return String::cast(*this)->ComputeAndSetHash(
      Heap::FromWritableHeapObject(*this)->isolate());
}

bool Name::IsInterestingSymbol() const {
  return IsSymbol() && Symbol::cast(*this)->is_interesting_symbol();
}

bool Name::IsPrivate() {
  return this->IsSymbol() && Symbol::cast(*this)->is_private();
}

bool Name::IsPrivateName() {
  bool is_private_name =
      this->IsSymbol() && Symbol::cast(*this)->is_private_name();
  DCHECK_IMPLIES(is_private_name, IsPrivate());
  return is_private_name;
}

bool Name::AsArrayIndex(uint32_t* index) {
  return IsString() && String::cast(*this)->AsArrayIndex(index);
}

// static
bool Name::ContainsCachedArrayIndex(uint32_t hash) {
  return (hash & Name::kDoesNotContainCachedArrayIndexMask) == 0;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_NAME_INL_H_
