// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_TABLE_INL_H_
#define V8_OBJECTS_HASH_TABLE_INL_H_

#include "src/heap/heap.h"
#include "src/objects/hash-table.h"

namespace v8 {
namespace internal {

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
int BaseShape<Key>::GetMapRootIndex() {
  return Heap::kHashTableMapRootIndex;
}

template <typename Derived, typename Shape>
int HashTable<Derived, Shape>::FindEntry(Key key) {
  return FindEntry(GetIsolate(), key);
}

template <typename Derived, typename Shape>
int HashTable<Derived, Shape>::FindEntry(Isolate* isolate, Key key) {
  return FindEntry(isolate, key, Shape::Hash(isolate, key));
}

// Find entry for key otherwise return kNotFound.
template <typename Derived, typename Shape>
int HashTable<Derived, Shape>::FindEntry(Isolate* isolate, Key key,
                                         int32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  Object* undefined = isolate->heap()->undefined_value();
  Object* the_hole = isolate->heap()->the_hole_value();
  USE(the_hole);
  while (true) {
    Object* element = KeyAt(entry);
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

template <typename KeyT>
bool BaseShape<KeyT>::IsLive(Isolate* isolate, Object* k) {
  Heap* heap = isolate->heap();
  return k != heap->the_hole_value() && k != heap->undefined_value();
}

template <typename Derived, typename Shape>
HashTable<Derived, Shape>* HashTable<Derived, Shape>::cast(Object* obj) {
  SLOW_DCHECK(obj->IsHashTable());
  return reinterpret_cast<HashTable*>(obj);
}

template <typename Derived, typename Shape>
const HashTable<Derived, Shape>* HashTable<Derived, Shape>::cast(
    const Object* obj) {
  SLOW_DCHECK(obj->IsHashTable());
  return reinterpret_cast<const HashTable*>(obj);
}

bool ObjectHashSet::Has(Isolate* isolate, Handle<Object> key, int32_t hash) {
  return FindEntry(isolate, key, hash) != kNotFound;
}

bool ObjectHashSet::Has(Isolate* isolate, Handle<Object> key) {
  Object* hash = key->GetHash();
  if (!hash->IsSmi()) return false;
  return FindEntry(isolate, key, Smi::ToInt(hash)) != kNotFound;
}

int OrderedHashSet::GetMapRootIndex() {
  return Heap::kOrderedHashSetMapRootIndex;
}

int OrderedHashMap::GetMapRootIndex() {
  return Heap::kOrderedHashMapMapRootIndex;
}

inline Object* OrderedHashMap::ValueAt(int entry) {
  DCHECK_LT(entry, this->UsedCapacity());
  return get(EntryToIndex(entry) + kValueOffset);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_HASH_TABLE_INL_H_
