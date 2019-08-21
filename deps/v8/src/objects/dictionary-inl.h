// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DICTIONARY_INL_H_
#define V8_OBJECTS_DICTIONARY_INL_H_

#include "src/objects/dictionary.h"

#include "src/numbers/hash-seed-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(GlobalDictionary)
CAST_ACCESSOR(NameDictionary)
CAST_ACCESSOR(NumberDictionary)
CAST_ACCESSOR(SimpleNumberDictionary)

template <typename Derived, typename Shape>
Dictionary<Derived, Shape>::Dictionary(Address ptr)
    : HashTable<Derived, Shape>(ptr) {}

template <typename Derived, typename Shape>
BaseNameDictionary<Derived, Shape>::BaseNameDictionary(Address ptr)
    : Dictionary<Derived, Shape>(ptr) {}

GlobalDictionary::GlobalDictionary(Address ptr)
    : BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>(ptr) {
  SLOW_DCHECK(IsGlobalDictionary());
}

NameDictionary::NameDictionary(Address ptr)
    : BaseNameDictionary<NameDictionary, NameDictionaryShape>(ptr) {
  SLOW_DCHECK(IsNameDictionary());
}

NumberDictionary::NumberDictionary(Address ptr)
    : Dictionary<NumberDictionary, NumberDictionaryShape>(ptr) {
  SLOW_DCHECK(IsNumberDictionary());
}

SimpleNumberDictionary::SimpleNumberDictionary(Address ptr)
    : Dictionary<SimpleNumberDictionary, SimpleNumberDictionaryShape>(ptr) {
  SLOW_DCHECK(IsSimpleNumberDictionary());
}

bool NumberDictionary::requires_slow_elements() {
  Object max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object.IsSmi()) return false;
  return 0 != (Smi::ToInt(max_index_object) & kRequiresSlowElementsMask);
}

uint32_t NumberDictionary::max_number_key() {
  DCHECK(!requires_slow_elements());
  Object max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object.IsSmi()) return 0;
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(max_index_object));
  return value >> kRequiresSlowElementsTagSize;
}

void NumberDictionary::set_requires_slow_elements() {
  set(kMaxNumberKeyIndex, Smi::FromInt(kRequiresSlowElementsMask));
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::ClearEntry(Isolate* isolate, int entry) {
  Object the_hole = this->GetReadOnlyRoots().the_hole_value();
  PropertyDetails details = PropertyDetails::Empty();
  Derived::cast(*this).SetEntry(isolate, entry, the_hole, the_hole, details);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::SetEntry(Isolate* isolate, int entry,
                                          Object key, Object value,
                                          PropertyDetails details) {
  DCHECK(Dictionary::kEntrySize == 2 || Dictionary::kEntrySize == 3);
  DCHECK(!key.IsName() || details.dictionary_index() > 0);
  int index = DerivedHashTable::EntryToIndex(entry);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = this->GetWriteBarrierMode(no_gc);
  this->set(index + Derived::kEntryKeyIndex, key, mode);
  this->set(index + Derived::kEntryValueIndex, value, mode);
  if (Shape::kHasDetails) DetailsAtPut(isolate, entry, details);
}

Object GlobalDictionaryShape::Unwrap(Object object) {
  return PropertyCell::cast(object).name();
}

RootIndex GlobalDictionaryShape::GetMapRootIndex() {
  return RootIndex::kGlobalDictionaryMap;
}

Name NameDictionary::NameAt(int entry) {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return NameAt(isolate, entry);
}

Name NameDictionary::NameAt(Isolate* isolate, int entry) {
  return Name::cast(KeyAt(isolate, entry));
}

RootIndex NameDictionaryShape::GetMapRootIndex() {
  return RootIndex::kNameDictionaryMap;
}

PropertyCell GlobalDictionary::CellAt(int entry) {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return CellAt(isolate, entry);
}

PropertyCell GlobalDictionary::CellAt(Isolate* isolate, int entry) {
  DCHECK(KeyAt(isolate, entry).IsPropertyCell(isolate));
  return PropertyCell::cast(KeyAt(isolate, entry));
}

bool GlobalDictionaryShape::IsLive(ReadOnlyRoots roots, Object k) {
  DCHECK_NE(roots.the_hole_value(), k);
  return k != roots.undefined_value();
}

bool GlobalDictionaryShape::IsKey(ReadOnlyRoots roots, Object k) {
  return IsLive(roots, k) && !PropertyCell::cast(k).value().IsTheHole(roots);
}

Name GlobalDictionary::NameAt(int entry) {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return NameAt(isolate, entry);
}

Name GlobalDictionary::NameAt(Isolate* isolate, int entry) {
  return CellAt(isolate, entry).name(isolate);
}

Object GlobalDictionary::ValueAt(int entry) {
  Isolate* isolate = GetIsolateForPtrCompr(*this);
  return ValueAt(isolate, entry);
}

Object GlobalDictionary::ValueAt(Isolate* isolate, int entry) {
  return CellAt(isolate, entry).value(isolate);
}

void GlobalDictionary::SetEntry(Isolate* isolate, int entry, Object key,
                                Object value, PropertyDetails details) {
  DCHECK_EQ(key, PropertyCell::cast(value).name());
  set(EntryToIndex(entry) + kEntryKeyIndex, value);
  DetailsAtPut(isolate, entry, details);
}

void GlobalDictionary::ValueAtPut(int entry, Object value) {
  set(EntryToIndex(entry), value);
}

bool NumberDictionaryBaseShape::IsMatch(uint32_t key, Object other) {
  DCHECK(other.IsNumber());
  return key == static_cast<uint32_t>(other.Number());
}

uint32_t NumberDictionaryBaseShape::Hash(Isolate* isolate, uint32_t key) {
  return ComputeSeededHash(key, HashSeed(isolate));
}

uint32_t NumberDictionaryBaseShape::HashForObject(ReadOnlyRoots roots,
                                                  Object other) {
  DCHECK(other.IsNumber());
  return ComputeSeededHash(static_cast<uint32_t>(other.Number()),
                           HashSeed(roots));
}

Handle<Object> NumberDictionaryBaseShape::AsHandle(Isolate* isolate,
                                                   uint32_t key) {
  return isolate->factory()->NewNumberFromUint(key);
}

RootIndex NumberDictionaryShape::GetMapRootIndex() {
  return RootIndex::kNumberDictionaryMap;
}

RootIndex SimpleNumberDictionaryShape::GetMapRootIndex() {
  return RootIndex::kSimpleNumberDictionaryMap;
}

bool NameDictionaryShape::IsMatch(Handle<Name> key, Object other) {
  DCHECK(other.IsTheHole() || Name::cast(other).IsUniqueName());
  DCHECK(key->IsUniqueName());
  return *key == other;
}

uint32_t NameDictionaryShape::Hash(Isolate* isolate, Handle<Name> key) {
  return key->Hash();
}

uint32_t NameDictionaryShape::HashForObject(ReadOnlyRoots roots, Object other) {
  return Name::cast(other).Hash();
}

bool GlobalDictionaryShape::IsMatch(Handle<Name> key, Object other) {
  DCHECK(PropertyCell::cast(other).name().IsUniqueName());
  return *key == PropertyCell::cast(other).name();
}

uint32_t GlobalDictionaryShape::HashForObject(ReadOnlyRoots roots,
                                              Object other) {
  return PropertyCell::cast(other).name().Hash();
}

Handle<Object> NameDictionaryShape::AsHandle(Isolate* isolate,
                                             Handle<Name> key) {
  DCHECK(key->IsUniqueName());
  return key;
}

template <typename Dictionary>
PropertyDetails GlobalDictionaryShape::DetailsAt(Dictionary dict, int entry) {
  DCHECK_LE(0, entry);  // Not found is -1, which is not caught by get().
  return dict.CellAt(entry).property_details();
}

template <typename Dictionary>
void GlobalDictionaryShape::DetailsAtPut(Isolate* isolate, Dictionary dict,
                                         int entry, PropertyDetails value) {
  DCHECK_LE(0, entry);  // Not found is -1, which is not caught by get().
  PropertyCell cell = dict.CellAt(entry);
  if (cell.property_details().IsReadOnly() != value.IsReadOnly()) {
    cell.dependent_code().DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
  cell.set_property_details(value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DICTIONARY_INL_H_
