// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DICTIONARY_INL_H_
#define V8_OBJECTS_DICTIONARY_INL_H_

#include <optional>

#include "src/execution/isolate-utils-inl.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/dictionary.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

template <typename Derived, typename Shape>
Tagged<Object> Dictionary<Derived, Shape>::ValueAt(InternalIndex entry) {
  PtrComprCageBase cage_base = GetPtrComprCageBase();
  return ValueAt(cage_base, entry);
}

template <typename Derived, typename Shape>
Tagged<Object> Dictionary<Derived, Shape>::ValueAt(PtrComprCageBase cage_base,
                                                   InternalIndex entry) {
  return this->get(DerivedHashTable::EntryToIndex(entry) +
                   Derived::kEntryValueIndex);
}

template <typename Derived, typename Shape>
Tagged<Object> Dictionary<Derived, Shape>::ValueAt(InternalIndex entry,
                                                   SeqCstAccessTag tag) {
  PtrComprCageBase cage_base = GetPtrComprCageBase();
  return ValueAt(cage_base, entry, tag);
}

template <typename Derived, typename Shape>
Tagged<Object> Dictionary<Derived, Shape>::ValueAt(PtrComprCageBase cage_base,
                                                   InternalIndex entry,
                                                   SeqCstAccessTag tag) {
  return this->get(
      DerivedHashTable::EntryToIndex(entry) + Derived::kEntryValueIndex, tag);
}

template <typename Derived, typename Shape>
std::optional<Tagged<Object>> Dictionary<Derived, Shape>::TryValueAt(
    InternalIndex entry) {
#if DEBUG
  Isolate* isolate;
  GetIsolateFromHeapObject(this, &isolate);
  DCHECK_NE(isolate, nullptr);
  SLOW_DCHECK(!isolate->heap()->IsPendingAllocation(Tagged(this)));
#endif  // DEBUG
  // We can read length() in a non-atomic way since we are reading an
  // initialized object which is not pending allocation.
  if (DerivedHashTable::EntryToIndex(entry) + Derived::kEntryValueIndex >=
      this->length()) {
    return {};
  }
  return ValueAt(entry);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::ValueAtPut(InternalIndex entry,
                                            Tagged<Object> value) {
  this->set(DerivedHashTable::EntryToIndex(entry) + Derived::kEntryValueIndex,
            value);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::ValueAtPut(InternalIndex entry,
                                            Tagged<Object> value,
                                            SeqCstAccessTag tag) {
  this->set(DerivedHashTable::EntryToIndex(entry) + Derived::kEntryValueIndex,
            value, tag);
}

template <typename Derived, typename Shape>
Tagged<Object> Dictionary<Derived, Shape>::ValueAtSwap(InternalIndex entry,
                                                       Tagged<Object> value,
                                                       SeqCstAccessTag tag) {
  return this->swap(
      DerivedHashTable::EntryToIndex(entry) + Derived::kEntryValueIndex, value,
      tag);
}

template <typename Derived, typename Shape>
Tagged<Object> Dictionary<Derived, Shape>::ValueAtCompareAndSwap(
    InternalIndex entry, Tagged<Object> expected, Tagged<Object> value,
    SeqCstAccessTag tag) {
  return this->compare_and_swap(
      DerivedHashTable::EntryToIndex(entry) + Derived::kEntryValueIndex,
      expected, value, tag);
}

template <typename Derived, typename Shape>
PropertyDetails Dictionary<Derived, Shape>::DetailsAt(InternalIndex entry) {
  return Shape::DetailsAt(Cast<Derived>(this), entry);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::DetailsAtPut(InternalIndex entry,
                                              PropertyDetails value) {
  Shape::DetailsAtPut(Cast<Derived>(this), entry, value);
}

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
  Tagged<Object> hash_obj = this->get(kObjectHashIndex);
  int hash = Smi::ToInt(hash_obj);
  DCHECK(PropertyArray::HashField::is_valid(hash));
  return hash;
}

bool NumberDictionary::requires_slow_elements() {
  Tagged<Object> max_index_object = get(kMaxNumberKeyIndex);
  if (!IsSmi(max_index_object)) return false;
  return 0 != (Smi::ToInt(max_index_object) & kRequiresSlowElementsMask);
}

uint32_t NumberDictionary::max_number_key() {
  DCHECK(!requires_slow_elements());
  Tagged<Object> max_index_object = get(kMaxNumberKeyIndex);
  if (!IsSmi(max_index_object)) return 0;
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(max_index_object));
  return value >> kRequiresSlowElementsTagSize;
}

void NumberDictionary::set_requires_slow_elements() {
  set(kMaxNumberKeyIndex, Smi::FromInt(kRequiresSlowElementsMask));
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::ClearEntry(InternalIndex entry) {
  Tagged<Object> the_hole = GetReadOnlyRoots().the_hole_value();
  PropertyDetails details = PropertyDetails::Empty();
  Cast<Derived>(this)->SetEntry(entry, the_hole, the_hole, details);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::SetEntry(InternalIndex entry,
                                          Tagged<Object> key,
                                          Tagged<Object> value,
                                          PropertyDetails details) {
  DCHECK(Dictionary::kEntrySize == 2 || Dictionary::kEntrySize == 3);
  DCHECK(!IsName(key) || details.dictionary_index() > 0);
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
PropertyDetails BaseDictionaryShape<Key>::DetailsAt(Tagged<Dictionary> dict,
                                                    InternalIndex entry) {
  static_assert(Dictionary::kEntrySize == 3);
  DCHECK(entry.is_found());
  return PropertyDetails(Cast<Smi>(dict->get(Dictionary::EntryToIndex(entry) +
                                             Dictionary::kEntryDetailsIndex)));
}

template <typename Key>
template <typename Dictionary>
void BaseDictionaryShape<Key>::DetailsAtPut(Tagged<Dictionary> dict,
                                            InternalIndex entry,
                                            PropertyDetails value) {
  static_assert(Dictionary::kEntrySize == 3);
  dict->set(Dictionary::EntryToIndex(entry) + Dictionary::kEntryDetailsIndex,
            value.AsSmi());
}

Tagged<Object> GlobalDictionaryShape::Unwrap(Tagged<Object> object) {
  return Cast<PropertyCell>(object)->name();
}

DirectHandle<Map> GlobalDictionary::GetMap(RootsTable& roots) {
  return roots.global_dictionary_map();
}

Tagged<Name> NameDictionary::NameAt(InternalIndex entry) {
  PtrComprCageBase cage_base = GetPtrComprCageBase();
  return NameAt(cage_base, entry);
}

Tagged<Name> NameDictionary::NameAt(PtrComprCageBase cage_base,
                                    InternalIndex entry) {
  return Cast<Name>(KeyAt(cage_base, entry));
}

DirectHandle<Map> NameDictionary::GetMap(RootsTable& roots) {
  return roots.name_dictionary_map();
}

uint32_t NameDictionary::flags() const {
  return Smi::ToInt(this->get(kFlagsIndex));
}

void NameDictionary::set_flags(uint32_t flags) {
  this->set(kFlagsIndex, Smi::FromInt(flags));
}

BIT_FIELD_ACCESSORS(NameDictionary, flags, may_have_interesting_properties,
                    NameDictionary::MayHaveInterestingPropertiesBit)

Tagged<PropertyCell> GlobalDictionary::CellAt(InternalIndex entry) {
  PtrComprCageBase cage_base = GetPtrComprCageBase();
  return CellAt(cage_base, entry);
}

Tagged<PropertyCell> GlobalDictionary::CellAt(PtrComprCageBase cage_base,
                                              InternalIndex entry) {
  DCHECK(IsPropertyCell(KeyAt(cage_base, entry), cage_base));
  return Cast<PropertyCell>(KeyAt(cage_base, entry));
}

Tagged<Name> GlobalDictionary::NameAt(InternalIndex entry) {
  PtrComprCageBase cage_base = GetPtrComprCageBase();
  return NameAt(cage_base, entry);
}

Tagged<Name> GlobalDictionary::NameAt(PtrComprCageBase cage_base,
                                      InternalIndex entry) {
  return CellAt(cage_base, entry)->name(cage_base);
}

Tagged<Object> GlobalDictionary::ValueAt(InternalIndex entry) {
  PtrComprCageBase cage_base = GetPtrComprCageBase();
  return ValueAt(cage_base, entry);
}

Tagged<Object> GlobalDictionary::ValueAt(PtrComprCageBase cage_base,
                                         InternalIndex entry) {
  return CellAt(cage_base, entry)->value(cage_base);
}

void GlobalDictionary::SetEntry(InternalIndex entry, Tagged<Object> key,
                                Tagged<Object> value, PropertyDetails details) {
  DCHECK_EQ(key, Cast<PropertyCell>(value)->name());
  set(EntryToIndex(entry) + kEntryKeyIndex, value);
  DetailsAtPut(entry, details);
}

void GlobalDictionary::ClearEntry(InternalIndex entry) {
  Tagged<Hole> the_hole = GetReadOnlyRoots().the_hole_value();
  set(EntryToIndex(entry) + kEntryKeyIndex, the_hole);
}

void GlobalDictionary::ValueAtPut(InternalIndex entry, Tagged<Object> value) {
  set(EntryToIndex(entry), value);
}

bool NumberDictionaryBaseShape::IsMatch(uint32_t key, Tagged<Object> other) {
  return key == static_cast<uint32_t>(Object::NumberValue(Cast<Number>(other)));
}

uint32_t NumberDictionaryBaseShape::Hash(ReadOnlyRoots roots, uint32_t key) {
  return ComputeSeededHash(key, HashSeed(roots));
}

uint32_t NumberDictionaryBaseShape::HashForObject(ReadOnlyRoots roots,
                                                  Tagged<Object> other) {
  DCHECK(IsNumber(other));
  return ComputeSeededHash(
      static_cast<uint32_t>(Object::NumberValue(Cast<Number>(other))),
      HashSeed(roots));
}

template <AllocationType allocation>
DirectHandle<Object> NumberDictionaryBaseShape::AsHandle(Isolate* isolate,
                                                         uint32_t key) {
  return isolate->factory()->NewNumberFromUint<allocation>(key);
}

template <AllocationType allocation>
DirectHandle<Object> NumberDictionaryBaseShape::AsHandle(LocalIsolate* isolate,
                                                         uint32_t key) {
  return isolate->factory()->NewNumberFromUint<allocation>(key);
}

DirectHandle<Map> NumberDictionary::GetMap(RootsTable& roots) {
  return roots.number_dictionary_map();
}

DirectHandle<Map> SimpleNumberDictionary::GetMap(RootsTable& roots) {
  return roots.simple_number_dictionary_map();
}

bool BaseNameDictionaryShape::IsMatch(DirectHandle<Name> key,
                                      Tagged<Object> other) {
  DCHECK(IsTheHole(other) || IsUniqueName(Cast<Name>(other)));
  DCHECK(IsUniqueName(*key));
  return *key == other;
}

uint32_t BaseNameDictionaryShape::Hash(ReadOnlyRoots roots,
                                       DirectHandle<Name> key) {
  DCHECK(IsUniqueName(*key));
  return key->hash();
}

uint32_t BaseNameDictionaryShape::HashForObject(ReadOnlyRoots roots,
                                                Tagged<Object> other) {
  DCHECK(IsUniqueName(other));
  return Cast<Name>(other)->hash();
}

bool GlobalDictionaryShape::IsMatch(DirectHandle<Name> key,
                                    Tagged<Object> other) {
  DCHECK(IsUniqueName(*key));
  DCHECK(IsUniqueName(Cast<PropertyCell>(other)->name()));
  return *key == Cast<PropertyCell>(other)->name();
}

uint32_t GlobalDictionaryShape::HashForObject(ReadOnlyRoots roots,
                                              Tagged<Object> other) {
  return Cast<PropertyCell>(other)->name()->hash();
}

template <AllocationType allocation>
DirectHandle<Object> BaseNameDictionaryShape::AsHandle(Isolate* isolate,
                                                       DirectHandle<Name> key) {
  DCHECK(IsUniqueName(*key));
  return key;
}

template <AllocationType allocation>
DirectHandle<Object> BaseNameDictionaryShape::AsHandle(LocalIsolate* isolate,
                                                       DirectHandle<Name> key) {
  DCHECK(IsUniqueName(*key));
  return key;
}

template <typename Dictionary>
PropertyDetails GlobalDictionaryShape::DetailsAt(Tagged<Dictionary> dict,
                                                 InternalIndex entry) {
  DCHECK(entry.is_found());
  return dict->CellAt(entry)->property_details();
}

template <typename Dictionary>
void GlobalDictionaryShape::DetailsAtPut(Tagged<Dictionary> dict,
                                         InternalIndex entry,
                                         PropertyDetails value) {
  DCHECK(entry.is_found());
  dict->CellAt(entry)->UpdatePropertyDetailsExceptCellType(value);
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DICTIONARY_INL_H_
