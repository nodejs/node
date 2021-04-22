// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DICTIONARY_INL_H_
#define V8_OBJECTS_DICTIONARY_INL_H_

#include "src/execution/isolate-utils-inl.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/dictionary.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/objects-inl.h"
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
Object Dictionary<Derived, Shape>::ValueAt(InternalIndex entry) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return ValueAt(isolate, entry);
}

template <typename Derived, typename Shape>
Object Dictionary<Derived, Shape>::ValueAt(IsolateRoot isolate,
                                           InternalIndex entry) {
  return this->get(isolate, DerivedHashTable::EntryToIndex(entry) +
                                Derived::kEntryValueIndex);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::ValueAtPut(InternalIndex entry, Object value) {
  this->set(DerivedHashTable::EntryToIndex(entry) + Derived::kEntryValueIndex,
            value);
}

template <typename Derived, typename Shape>
PropertyDetails Dictionary<Derived, Shape>::DetailsAt(InternalIndex entry) {
  return Shape::DetailsAt(Derived::cast(*this), entry);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::DetailsAtPut(InternalIndex entry,
                                              PropertyDetails value) {
  Shape::DetailsAtPut(Derived::cast(*this), entry, value);
}

template <typename Derived, typename Shape>
BaseNameDictionary<Derived, Shape>::BaseNameDictionary(Address ptr)
    : Dictionary<Derived, Shape>(ptr) {}

template <typename Derived, typename Shape>
void BaseNameDictionary<Derived, Shape>::set_next_enumeration_index(int index) {
  DCHECK_LT(0, index);
  this->set(kNextEnumerationIndexIndex, Smi::FromInt(index));
}

template <typename Derived, typename Shape>
int BaseNameDictionary<Derived, Shape>::next_enumeration_index() {
  return Smi::ToInt(this->get(kNextEnumerationIndexIndex));
}

template <typename Derived, typename Shape>
void BaseNameDictionary<Derived, Shape>::SetHash(int hash) {
  DCHECK(PropertyArray::HashField::is_valid(hash));
  this->set(kObjectHashIndex, Smi::FromInt(hash));
}

template <typename Derived, typename Shape>
int BaseNameDictionary<Derived, Shape>::Hash() const {
  Object hash_obj = this->get(kObjectHashIndex);
  int hash = Smi::ToInt(hash_obj);
  DCHECK(PropertyArray::HashField::is_valid(hash));
  return hash;
}

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
void Dictionary<Derived, Shape>::ClearEntry(InternalIndex entry) {
  Object the_hole = this->GetReadOnlyRoots().the_hole_value();
  PropertyDetails details = PropertyDetails::Empty();
  Derived::cast(*this).SetEntry(entry, the_hole, the_hole, details);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::SetEntry(InternalIndex entry, Object key,
                                          Object value,
                                          PropertyDetails details) {
  DCHECK(Dictionary::kEntrySize == 2 || Dictionary::kEntrySize == 3);
  DCHECK(!key.IsName() || details.dictionary_index() > 0);
  int index = DerivedHashTable::EntryToIndex(entry);
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = this->GetWriteBarrierMode(no_gc);
  this->set(index + Derived::kEntryKeyIndex, key, mode);
  this->set(index + Derived::kEntryValueIndex, value, mode);
  if (Shape::kHasDetails) DetailsAtPut(entry, details);
}

template <typename Derived, typename Shape>
ObjectSlot Dictionary<Derived, Shape>::RawFieldOfValueAt(InternalIndex entry) {
  return this->RawFieldOfElementAt(DerivedHashTable::EntryToIndex(entry) +
                                   Derived::kEntryValueIndex);
}

template <typename Key>
template <typename Dictionary>
PropertyDetails BaseDictionaryShape<Key>::DetailsAt(Dictionary dict,
                                                    InternalIndex entry) {
  STATIC_ASSERT(Dictionary::kEntrySize == 3);
  DCHECK(entry.is_found());
  return PropertyDetails(Smi::cast(dict.get(Dictionary::EntryToIndex(entry) +
                                            Dictionary::kEntryDetailsIndex)));
}

template <typename Key>
template <typename Dictionary>
void BaseDictionaryShape<Key>::DetailsAtPut(Dictionary dict,
                                            InternalIndex entry,
                                            PropertyDetails value) {
  STATIC_ASSERT(Dictionary::kEntrySize == 3);
  dict.set(Dictionary::EntryToIndex(entry) + Dictionary::kEntryDetailsIndex,
           value.AsSmi());
}

Object GlobalDictionaryShape::Unwrap(Object object) {
  return PropertyCell::cast(object).name();
}

Handle<Map> GlobalDictionary::GetMap(ReadOnlyRoots roots) {
  return roots.global_dictionary_map_handle();
}

Name NameDictionary::NameAt(InternalIndex entry) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return NameAt(isolate, entry);
}

Name NameDictionary::NameAt(IsolateRoot isolate, InternalIndex entry) {
  return Name::cast(KeyAt(isolate, entry));
}

Handle<Map> NameDictionary::GetMap(ReadOnlyRoots roots) {
  return roots.name_dictionary_map_handle();
}

PropertyCell GlobalDictionary::CellAt(InternalIndex entry) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return CellAt(isolate, entry);
}

PropertyCell GlobalDictionary::CellAt(IsolateRoot isolate,
                                      InternalIndex entry) {
  DCHECK(KeyAt(isolate, entry).IsPropertyCell(isolate));
  return PropertyCell::cast(KeyAt(isolate, entry));
}

Name GlobalDictionary::NameAt(InternalIndex entry) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return NameAt(isolate, entry);
}

Name GlobalDictionary::NameAt(IsolateRoot isolate, InternalIndex entry) {
  return CellAt(isolate, entry).name(isolate);
}

Object GlobalDictionary::ValueAt(InternalIndex entry) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return ValueAt(isolate, entry);
}

Object GlobalDictionary::ValueAt(IsolateRoot isolate, InternalIndex entry) {
  return CellAt(isolate, entry).value(isolate);
}

void GlobalDictionary::SetEntry(InternalIndex entry, Object key, Object value,
                                PropertyDetails details) {
  DCHECK_EQ(key, PropertyCell::cast(value).name());
  set(EntryToIndex(entry) + kEntryKeyIndex, value);
  DetailsAtPut(entry, details);
}

void GlobalDictionary::ClearEntry(InternalIndex entry) {
  Object the_hole = this->GetReadOnlyRoots().the_hole_value();
  set(EntryToIndex(entry) + kEntryKeyIndex, the_hole);
}

void GlobalDictionary::ValueAtPut(InternalIndex entry, Object value) {
  set(EntryToIndex(entry), value);
}

bool NumberDictionaryBaseShape::IsMatch(uint32_t key, Object other) {
  DCHECK(other.IsNumber());
  return key == static_cast<uint32_t>(other.Number());
}

uint32_t NumberDictionaryBaseShape::Hash(ReadOnlyRoots roots, uint32_t key) {
  return ComputeSeededHash(key, HashSeed(roots));
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

Handle<Object> NumberDictionaryBaseShape::AsHandle(LocalIsolate* isolate,
                                                   uint32_t key) {
  return isolate->factory()->NewNumberFromUint<AllocationType::kOld>(key);
}

Handle<Map> NumberDictionary::GetMap(ReadOnlyRoots roots) {
  return roots.number_dictionary_map_handle();
}

Handle<Map> SimpleNumberDictionary::GetMap(ReadOnlyRoots roots) {
  return roots.simple_number_dictionary_map_handle();
}

bool NameDictionaryShape::IsMatch(Handle<Name> key, Object other) {
  DCHECK(other.IsTheHole() || Name::cast(other).IsUniqueName());
  DCHECK(key->IsUniqueName());
  return *key == other;
}

uint32_t NameDictionaryShape::Hash(ReadOnlyRoots roots, Handle<Name> key) {
  DCHECK(key->IsUniqueName());
  return key->hash();
}

uint32_t NameDictionaryShape::HashForObject(ReadOnlyRoots roots, Object other) {
  DCHECK(other.IsUniqueName());
  return Name::cast(other).hash();
}

bool GlobalDictionaryShape::IsMatch(Handle<Name> key, Object other) {
  DCHECK(key->IsUniqueName());
  DCHECK(PropertyCell::cast(other).name().IsUniqueName());
  return *key == PropertyCell::cast(other).name();
}

uint32_t GlobalDictionaryShape::HashForObject(ReadOnlyRoots roots,
                                              Object other) {
  return PropertyCell::cast(other).name().hash();
}

Handle<Object> NameDictionaryShape::AsHandle(Isolate* isolate,
                                             Handle<Name> key) {
  DCHECK(key->IsUniqueName());
  return key;
}

Handle<Object> NameDictionaryShape::AsHandle(LocalIsolate* isolate,
                                             Handle<Name> key) {
  DCHECK(key->IsUniqueName());
  return key;
}

template <typename Dictionary>
PropertyDetails GlobalDictionaryShape::DetailsAt(Dictionary dict,
                                                 InternalIndex entry) {
  DCHECK(entry.is_found());
  return dict.CellAt(entry).property_details();
}

template <typename Dictionary>
void GlobalDictionaryShape::DetailsAtPut(Dictionary dict, InternalIndex entry,
                                         PropertyDetails value) {
  DCHECK(entry.is_found());
  dict.CellAt(entry).UpdatePropertyDetailsExceptCellType(value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DICTIONARY_INL_H_
