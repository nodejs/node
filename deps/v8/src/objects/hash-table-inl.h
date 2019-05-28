// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_TABLE_INL_H_
#define V8_OBJECTS_HASH_TABLE_INL_H_

#include "src/objects/hash-table.h"

#include "src/heap/heap.h"
#include "src/objects-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(HashTableBase, FixedArray)

template <typename Derived, typename Shape>
HashTable<Derived, Shape>::HashTable(Address ptr) : HashTableBase(ptr) {
  SLOW_DCHECK(IsHashTable());
}

template <typename Derived, typename Shape>
ObjectHashTableBase<Derived, Shape>::ObjectHashTableBase(Address ptr)
    : HashTable<Derived, Shape>(ptr) {}

ObjectHashTable::ObjectHashTable(Address ptr)
    : ObjectHashTableBase<ObjectHashTable, ObjectHashTableShape>(ptr) {
  SLOW_DCHECK(IsObjectHashTable());
}

EphemeronHashTable::EphemeronHashTable(Address ptr)
    : ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape>(ptr) {
  SLOW_DCHECK(IsEphemeronHashTable());
}

ObjectHashSet::ObjectHashSet(Address ptr)
    : HashTable<ObjectHashSet, ObjectHashSetShape>(ptr) {
  SLOW_DCHECK(IsObjectHashSet());
}

CAST_ACCESSOR(ObjectHashTable)
CAST_ACCESSOR(EphemeronHashTable)
CAST_ACCESSOR(ObjectHashSet)

void EphemeronHashTable::set_key(int index, Object value) {
  DCHECK_NE(GetReadOnlyRoots().fixed_cow_array_map(), map());
  DCHECK(IsEphemeronHashTable());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kTaggedSize;
  RELAXED_WRITE_FIELD(*this, offset, value);
  EPHEMERON_KEY_WRITE_BARRIER(*this, offset, value);
}

void EphemeronHashTable::set_key(int index, Object value,
                                 WriteBarrierMode mode) {
  DCHECK_NE(GetReadOnlyRoots().fixed_cow_array_map(), map());
  DCHECK(IsEphemeronHashTable());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kTaggedSize;
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_EPHEMERON_KEY_WRITE_BARRIER(*this, offset, value, mode);
}

int HashTableBase::NumberOfElements() const {
  return Smi::ToInt(get(kNumberOfElementsIndex));
}

int HashTableBase::NumberOfDeletedElements() const {
  return Smi::ToInt(get(kNumberOfDeletedElementsIndex));
}

int HashTableBase::Capacity() const { return Smi::ToInt(get(kCapacityIndex)); }

void HashTableBase::ElementAdded() {
  SetNumberOfElements(NumberOfElements() + 1);
}

void HashTableBase::ElementRemoved() {
  SetNumberOfElements(NumberOfElements() - 1);
  SetNumberOfDeletedElements(NumberOfDeletedElements() + 1);
}

void HashTableBase::ElementsRemoved(int n) {
  SetNumberOfElements(NumberOfElements() - n);
  SetNumberOfDeletedElements(NumberOfDeletedElements() + n);
}

// static
int HashTableBase::ComputeCapacity(int at_least_space_for) {
  // Add 50% slack to make slot collisions sufficiently unlikely.
  // See matching computation in HashTable::HasSufficientCapacityToAdd().
  // Must be kept in sync with CodeStubAssembler::HashTableComputeCapacity().
  int raw_cap = at_least_space_for + (at_least_space_for >> 1);
  int capacity = base::bits::RoundUpToPowerOfTwo32(raw_cap);
  return Max(capacity, kMinCapacity);
}

void HashTableBase::SetNumberOfElements(int nof) {
  set(kNumberOfElementsIndex, Smi::FromInt(nof));
}

void HashTableBase::SetNumberOfDeletedElements(int nod) {
  set(kNumberOfDeletedElementsIndex, Smi::FromInt(nod));
}

template <typename Key>
RootIndex BaseShape<Key>::GetMapRootIndex() {
  return RootIndex::kHashTableMap;
}

RootIndex EphemeronHashTableShape::GetMapRootIndex() {
  return RootIndex::kEphemeronHashTableMap;
}

template <typename Derived, typename Shape>
int HashTable<Derived, Shape>::FindEntry(Isolate* isolate, Key key) {
  return FindEntry(ReadOnlyRoots(isolate), key, Shape::Hash(isolate, key));
}

// Find entry for key otherwise return kNotFound.
template <typename Derived, typename Shape>
int HashTable<Derived, Shape>::FindEntry(ReadOnlyRoots roots, Key key,
                                         int32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  Object undefined = roots.undefined_value();
  Object the_hole = roots.the_hole_value();
  USE(the_hole);
  while (true) {
    Object element = KeyAt(entry);
    // Empty entry. Uses raw unchecked accessors because it is called by the
    // string table during bootstrapping.
    if (element == undefined) break;
    if (!(Shape::kNeedsHoleCheck && the_hole == element)) {
      if (Shape::IsMatch(key, element)) return entry;
    }
    entry = NextProbe(entry, count++, capacity);
  }
  return kNotFound;
}

template <typename Derived, typename Shape>
bool HashTable<Derived, Shape>::IsKey(ReadOnlyRoots roots, Object k) {
  return Shape::IsKey(roots, k);
}

template <typename Derived, typename Shape>
bool HashTable<Derived, Shape>::ToKey(ReadOnlyRoots roots, int entry,
                                      Object* out_k) {
  Object k = KeyAt(entry);
  if (!IsKey(roots, k)) return false;
  *out_k = Shape::Unwrap(k);
  return true;
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::set_key(int index, Object value) {
  DCHECK(!IsEphemeronHashTable());
  FixedArray::set(index, value);
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::set_key(int index, Object value,
                                        WriteBarrierMode mode) {
  DCHECK(!IsEphemeronHashTable());
  FixedArray::set(index, value, mode);
}

template <typename KeyT>
bool BaseShape<KeyT>::IsKey(ReadOnlyRoots roots, Object key) {
  return IsLive(roots, key);
}

template <typename KeyT>
bool BaseShape<KeyT>::IsLive(ReadOnlyRoots roots, Object k) {
  return k != roots.the_hole_value() && k != roots.undefined_value();
}

bool ObjectHashSet::Has(Isolate* isolate, Handle<Object> key, int32_t hash) {
  return FindEntry(ReadOnlyRoots(isolate), key, hash) != kNotFound;
}

bool ObjectHashSet::Has(Isolate* isolate, Handle<Object> key) {
  Object hash = key->GetHash();
  if (!hash->IsSmi()) return false;
  return FindEntry(ReadOnlyRoots(isolate), key, Smi::ToInt(hash)) != kNotFound;
}

bool ObjectHashTableShape::IsMatch(Handle<Object> key, Object other) {
  return key->SameValue(other);
}

uint32_t ObjectHashTableShape::Hash(Isolate* isolate, Handle<Object> key) {
  return Smi::ToInt(key->GetHash());
}

uint32_t ObjectHashTableShape::HashForObject(ReadOnlyRoots roots,
                                             Object other) {
  return Smi::ToInt(other->GetHash());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HASH_TABLE_INL_H_
