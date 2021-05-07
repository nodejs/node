// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NAME_INL_H_
#define V8_OBJECTS_NAME_INL_H_

#include "src/base/logging.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/name.h"
#include "src/objects/primitive-heap-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/name-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(Name)
TQ_OBJECT_CONSTRUCTORS_IMPL(Symbol)

BIT_FIELD_ACCESSORS(Symbol, flags, is_private, Symbol::IsPrivateBit)
BIT_FIELD_ACCESSORS(Symbol, flags, is_well_known_symbol,
                    Symbol::IsWellKnownSymbolBit)
BIT_FIELD_ACCESSORS(Symbol, flags, is_in_public_symbol_table,
                    Symbol::IsInPublicSymbolTableBit)
BIT_FIELD_ACCESSORS(Symbol, flags, is_interesting_symbol,
                    Symbol::IsInterestingSymbolBit)

bool Symbol::is_private_brand() const {
  bool value = Symbol::IsPrivateBrandBit::decode(flags());
  DCHECK_IMPLIES(value, is_private());
  return value;
}

void Symbol::set_is_private_brand() {
  set_flags(Symbol::IsPrivateBit::update(flags(), true));
  set_flags(Symbol::IsPrivateNameBit::update(flags(), true));
  set_flags(Symbol::IsPrivateBrandBit::update(flags(), true));
}

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

DEF_GETTER(Name, IsUniqueName, bool) {
  uint32_t type = map(isolate).instance_type();
  bool result = (type & (kIsNotStringMask | kIsNotInternalizedMask)) !=
                (kStringTag | kNotInternalizedTag);
  SLOW_DCHECK(result == HeapObject::IsUniqueName());
  DCHECK_IMPLIES(result, HasHashCode());
  return result;
}

bool Name::Equals(Name other) {
  if (other == *this) return true;
  if ((this->IsInternalizedString() && other.IsInternalizedString()) ||
      this->IsSymbol() || other.IsSymbol()) {
    return false;
  }
  return String::cast(*this).SlowEquals(String::cast(other));
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

bool Name::IsHashFieldComputed(uint32_t raw_hash_field) {
  return (raw_hash_field & kHashNotComputedMask) == 0;
}

bool Name::HasHashCode() const { return IsHashFieldComputed(raw_hash_field()); }

uint32_t Name::EnsureHash() {
  // Fast case: has hash code already been computed?
  uint32_t field = raw_hash_field();
  if (IsHashFieldComputed(field)) return field >> kHashShift;
  // Slow case: compute hash code and set it. Has to be a string.
  return String::cast(*this).ComputeAndSetHash();
}

uint32_t Name::hash() const {
  uint32_t field = raw_hash_field();
  DCHECK(IsHashFieldComputed(field));
  return field >> kHashShift;
}

DEF_GETTER(Name, IsInterestingSymbol, bool) {
  return IsSymbol(isolate) && Symbol::cast(*this).is_interesting_symbol();
}

DEF_GETTER(Name, IsPrivate, bool) {
  return this->IsSymbol(isolate) && Symbol::cast(*this).is_private();
}

DEF_GETTER(Name, IsPrivateName, bool) {
  bool is_private_name =
      this->IsSymbol(isolate) && Symbol::cast(*this).is_private_name();
  DCHECK_IMPLIES(is_private_name, IsPrivate());
  return is_private_name;
}

DEF_GETTER(Name, IsPrivateBrand, bool) {
  bool is_private_brand =
      this->IsSymbol(isolate) && Symbol::cast(*this).is_private_brand();
  DCHECK_IMPLIES(is_private_brand, IsPrivateName());
  return is_private_brand;
}

bool Name::AsArrayIndex(uint32_t* index) {
  return IsString() && String::cast(*this).AsArrayIndex(index);
}

bool Name::AsIntegerIndex(size_t* index) {
  return IsString() && String::cast(*this).AsIntegerIndex(index);
}

// static
bool Name::ContainsCachedArrayIndex(uint32_t raw_hash_field) {
  return (raw_hash_field & Name::kDoesNotContainCachedArrayIndexMask) == 0;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_NAME_INL_H_
