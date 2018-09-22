// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/ordered-hash-table.h"

#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/ordered-hash-table-inl.h"

namespace v8 {
namespace internal {

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure) {
  // Capacity must be a power of two, since we depend on being able
  // to divide and multiple by 2 (kLoadFactor) to derive capacity
  // from number of buckets. If we decide to change kLoadFactor
  // to something other than 2, capacity should be stored as another
  // field of this object.
  capacity = base::bits::RoundUpToPowerOfTwo32(Max(kMinCapacity, capacity));
  if (capacity > kMaxCapacity) {
    isolate->heap()->FatalProcessOutOfMemory("invalid table size");
  }
  int num_buckets = capacity / kLoadFactor;
  Handle<FixedArray> backing_store = isolate->factory()->NewFixedArrayWithMap(
      static_cast<Heap::RootListIndex>(Derived::GetMapRootIndex()),
      kHashTableStartIndex + num_buckets + (capacity * kEntrySize), pretenure);
  Handle<Derived> table = Handle<Derived>::cast(backing_store);
  for (int i = 0; i < num_buckets; ++i) {
    table->set(kHashTableStartIndex + i, Smi::FromInt(kNotFound));
  }
  table->SetNumberOfBuckets(num_buckets);
  table->SetNumberOfElements(0);
  table->SetNumberOfDeletedElements(0);
  return table;
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::EnsureGrowable(
    Isolate* isolate, Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int capacity = table->Capacity();
  if ((nof + nod) < capacity) return table;
  // Don't need to grow if we can simply clear out deleted entries instead.
  // Note that we can't compact in place, though, so we always allocate
  // a new table.
  return Rehash(isolate, table,
                (nod < (capacity >> 1)) ? capacity << 1 : capacity);
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Shrink(
    Isolate* isolate, Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int capacity = table->Capacity();
  if (nof >= (capacity >> 2)) return table;
  return Rehash(isolate, table, capacity / 2);
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Clear(
    Isolate* isolate, Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table = Allocate(
      isolate, kMinCapacity, Heap::InNewSpace(*table) ? NOT_TENURED : TENURED);

  table->SetNextTable(*new_table);
  table->SetNumberOfDeletedElements(kClearedTableSentinel);

  return new_table;
}

template <class Derived, int entrysize>
bool OrderedHashTable<Derived, entrysize>::HasKey(Isolate* isolate,
                                                  Derived* table, Object* key) {
  DCHECK((entrysize == 1 && table->IsOrderedHashSet()) ||
         (entrysize == 2 && table->IsOrderedHashMap()));
  DisallowHeapAllocation no_gc;
  int entry = table->FindEntry(isolate, key);
  return entry != kNotFound;
}

Handle<OrderedHashSet> OrderedHashSet::Add(Isolate* isolate,
                                           Handle<OrderedHashSet> table,
                                           Handle<Object> key) {
  int hash = key->GetOrCreateHash(isolate)->value();
  int entry = table->HashToEntry(hash);
  // Walk the chain of the bucket and try finding the key.
  while (entry != kNotFound) {
    Object* candidate_key = table->KeyAt(entry);
    // Do not add if we have the key already
    if (candidate_key->SameValueZero(*key)) return table;
    entry = table->NextChainEntry(entry);
  }

  table = OrderedHashSet::EnsureGrowable(isolate, table);
  // Read the existing bucket values.
  int bucket = table->HashToBucket(hash);
  int previous_entry = table->HashToEntry(hash);
  int nof = table->NumberOfElements();
  // Insert a new entry at the end,
  int new_entry = nof + table->NumberOfDeletedElements();
  int new_index = table->EntryToIndex(new_entry);
  table->set(new_index, *key);
  table->set(new_index + kChainOffset, Smi::FromInt(previous_entry));
  // and point the bucket to the new entry.
  table->set(kHashTableStartIndex + bucket, Smi::FromInt(new_entry));
  table->SetNumberOfElements(nof + 1);
  return table;
}

Handle<FixedArray> OrderedHashSet::ConvertToKeysArray(
    Isolate* isolate, Handle<OrderedHashSet> table, GetKeysConversion convert) {
  int length = table->NumberOfElements();
  int nof_buckets = table->NumberOfBuckets();
  // Convert the dictionary to a linear list.
  Handle<FixedArray> result = Handle<FixedArray>::cast(table);
  // From this point on table is no longer a valid OrderedHashSet.
  result->set_map(ReadOnlyRoots(isolate).fixed_array_map());
  int const kMaxStringTableEntries =
      isolate->heap()->MaxNumberToStringCacheSize();
  for (int i = 0; i < length; i++) {
    int index = kHashTableStartIndex + nof_buckets + (i * kEntrySize);
    Object* key = table->get(index);
    if (convert == GetKeysConversion::kConvertToString) {
      uint32_t index_value;
      if (key->ToArrayIndex(&index_value)) {
        // Avoid trashing the Number2String cache if indices get very large.
        bool use_cache = i < kMaxStringTableEntries;
        key = *isolate->factory()->Uint32ToString(index_value, use_cache);
      } else {
        CHECK(key->IsName());
      }
    }
    result->set(i, key);
  }
  return FixedArray::ShrinkOrEmpty(isolate, result, length);
}

HeapObject* OrderedHashSet::GetEmpty(ReadOnlyRoots ro_roots) {
  return ro_roots.empty_ordered_hash_set();
}

HeapObject* OrderedHashMap::GetEmpty(ReadOnlyRoots ro_roots) {
  return ro_roots.empty_ordered_hash_map();
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Rehash(
    Isolate* isolate, Handle<Derived> table, int new_capacity) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table = Allocate(
      isolate, new_capacity, Heap::InNewSpace(*table) ? NOT_TENURED : TENURED);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int new_buckets = new_table->NumberOfBuckets();
  int new_entry = 0;
  int removed_holes_index = 0;

  DisallowHeapAllocation no_gc;
  for (int old_entry = 0; old_entry < (nof + nod); ++old_entry) {
    Object* key = table->KeyAt(old_entry);
    if (key->IsTheHole(isolate)) {
      table->SetRemovedIndexAt(removed_holes_index++, old_entry);
      continue;
    }

    Object* hash = key->GetHash();
    int bucket = Smi::ToInt(hash) & (new_buckets - 1);
    Object* chain_entry = new_table->get(kHashTableStartIndex + bucket);
    new_table->set(kHashTableStartIndex + bucket, Smi::FromInt(new_entry));
    int new_index = new_table->EntryToIndex(new_entry);
    int old_index = table->EntryToIndex(old_entry);
    for (int i = 0; i < entrysize; ++i) {
      Object* value = table->get(old_index + i);
      new_table->set(new_index + i, value);
    }
    new_table->set(new_index + kChainOffset, chain_entry);
    ++new_entry;
  }

  DCHECK_EQ(nod, removed_holes_index);

  new_table->SetNumberOfElements(nof);
  table->SetNextTable(*new_table);

  return new_table;
}

template <class Derived, int entrysize>
bool OrderedHashTable<Derived, entrysize>::Delete(Isolate* isolate,
                                                  Derived* table, Object* key) {
  DisallowHeapAllocation no_gc;
  int entry = table->FindEntry(isolate, key);
  if (entry == kNotFound) return false;

  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int index = table->EntryToIndex(entry);

  Object* hole = ReadOnlyRoots(isolate).the_hole_value();
  for (int i = 0; i < entrysize; ++i) {
    table->set(index + i, hole);
  }

  table->SetNumberOfElements(nof - 1);
  table->SetNumberOfDeletedElements(nod + 1);

  return true;
}

Object* OrderedHashMap::GetHash(Isolate* isolate, Object* key) {
  DisallowHeapAllocation no_gc;

  Object* hash = key->GetHash();
  // If the object does not have an identity hash, it was never used as a key
  if (hash->IsUndefined(isolate)) return Smi::FromInt(-1);
  DCHECK(hash->IsSmi());
  DCHECK_GE(Smi::cast(hash)->value(), 0);
  return hash;
}

Handle<OrderedHashMap> OrderedHashMap::Add(Isolate* isolate,
                                           Handle<OrderedHashMap> table,
                                           Handle<Object> key,
                                           Handle<Object> value) {
  int hash = key->GetOrCreateHash(isolate)->value();
  int entry = table->HashToEntry(hash);
  // Walk the chain of the bucket and try finding the key.
  {
    DisallowHeapAllocation no_gc;
    Object* raw_key = *key;
    while (entry != kNotFound) {
      Object* candidate_key = table->KeyAt(entry);
      // Do not add if we have the key already
      if (candidate_key->SameValueZero(raw_key)) return table;
      entry = table->NextChainEntry(entry);
    }
  }

  table = OrderedHashMap::EnsureGrowable(isolate, table);
  // Read the existing bucket values.
  int bucket = table->HashToBucket(hash);
  int previous_entry = table->HashToEntry(hash);
  int nof = table->NumberOfElements();
  // Insert a new entry at the end,
  int new_entry = nof + table->NumberOfDeletedElements();
  int new_index = table->EntryToIndex(new_entry);
  table->set(new_index, *key);
  table->set(new_index + kValueOffset, *value);
  table->set(new_index + kChainOffset, Smi::FromInt(previous_entry));
  // and point the bucket to the new entry.
  table->set(kHashTableStartIndex + bucket, Smi::FromInt(new_entry));
  table->SetNumberOfElements(nof + 1);
  return table;
}

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, 1>::EnsureGrowable(
    Isolate* isolate, Handle<OrderedHashSet> table);

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Shrink(
    Isolate* isolate, Handle<OrderedHashSet> table);

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Clear(
    Isolate* isolate, Handle<OrderedHashSet> table);

template bool OrderedHashTable<OrderedHashSet, 1>::HasKey(Isolate* isolate,
                                                          OrderedHashSet* table,
                                                          Object* key);

template bool OrderedHashTable<OrderedHashSet, 1>::Delete(Isolate* isolate,
                                                          OrderedHashSet* table,
                                                          Object* key);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, 2>::EnsureGrowable(
    Isolate* isolate, Handle<OrderedHashMap> table);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Shrink(
    Isolate* isolate, Handle<OrderedHashMap> table);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Clear(
    Isolate* isolate, Handle<OrderedHashMap> table);

template bool OrderedHashTable<OrderedHashMap, 2>::HasKey(Isolate* isolate,
                                                          OrderedHashMap* table,
                                                          Object* key);

template bool OrderedHashTable<OrderedHashMap, 2>::Delete(Isolate* isolate,
                                                          OrderedHashMap* table,
                                                          Object* key);

template <>
Handle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Allocate(Isolate* isolate,
                                                     int capacity,
                                                     PretenureFlag pretenure) {
  return isolate->factory()->NewSmallOrderedHashSet(capacity, pretenure);
}

template <>
Handle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Allocate(Isolate* isolate,
                                                     int capacity,
                                                     PretenureFlag pretenure) {
  return isolate->factory()->NewSmallOrderedHashMap(capacity, pretenure);
}

template <class Derived>
void SmallOrderedHashTable<Derived>::Initialize(Isolate* isolate,
                                                int capacity) {
  DisallowHeapAllocation no_gc;
  int num_buckets = capacity / kLoadFactor;
  int num_chains = capacity;

  SetNumberOfBuckets(num_buckets);
  SetNumberOfElements(0);
  SetNumberOfDeletedElements(0);

  Address hashtable_start = GetHashTableStartAddress(capacity);
  memset(reinterpret_cast<byte*>(hashtable_start), kNotFound,
         num_buckets + num_chains);

  if (Heap::InNewSpace(this)) {
    MemsetPointer(RawField(this, kDataTableStartOffset),
                  ReadOnlyRoots(isolate).the_hole_value(),
                  capacity * Derived::kEntrySize);
  } else {
    for (int i = 0; i < capacity; i++) {
      for (int j = 0; j < Derived::kEntrySize; j++) {
        SetDataEntry(i, j, ReadOnlyRoots(isolate).the_hole_value());
      }
    }
  }

#ifdef DEBUG
  for (int i = 0; i < num_buckets; ++i) {
    DCHECK_EQ(kNotFound, GetFirstEntry(i));
  }

  for (int i = 0; i < num_chains; ++i) {
    DCHECK_EQ(kNotFound, GetNextEntry(i));
  }

  for (int i = 0; i < capacity; ++i) {
    for (int j = 0; j < Derived::kEntrySize; j++) {
      DCHECK_EQ(ReadOnlyRoots(isolate).the_hole_value(), GetDataEntry(i, j));
    }
  }
#endif  // DEBUG
}

MaybeHandle<SmallOrderedHashSet> SmallOrderedHashSet::Add(
    Isolate* isolate, Handle<SmallOrderedHashSet> table, Handle<Object> key) {
  if (table->HasKey(isolate, key)) return table;

  if (table->UsedCapacity() >= table->Capacity()) {
    MaybeHandle<SmallOrderedHashSet> new_table =
        SmallOrderedHashSet::Grow(isolate, table);
    if (!new_table.ToHandle(&table)) {
      return MaybeHandle<SmallOrderedHashSet>();
    }
  }

  int hash = key->GetOrCreateHash(isolate)->value();
  int nof = table->NumberOfElements();

  // Read the existing bucket values.
  int bucket = table->HashToBucket(hash);
  int previous_entry = table->HashToFirstEntry(hash);

  // Insert a new entry at the end,
  int new_entry = nof + table->NumberOfDeletedElements();

  table->SetDataEntry(new_entry, SmallOrderedHashSet::kKeyIndex, *key);
  table->SetFirstEntry(bucket, new_entry);
  table->SetNextEntry(new_entry, previous_entry);

  // and update book keeping.
  table->SetNumberOfElements(nof + 1);

  return table;
}

MaybeHandle<SmallOrderedHashMap> SmallOrderedHashMap::Add(
    Isolate* isolate, Handle<SmallOrderedHashMap> table, Handle<Object> key,
    Handle<Object> value) {
  if (table->HasKey(isolate, key)) return table;

  if (table->UsedCapacity() >= table->Capacity()) {
    MaybeHandle<SmallOrderedHashMap> new_table =
        SmallOrderedHashMap::Grow(isolate, table);
    if (!new_table.ToHandle(&table)) {
      return MaybeHandle<SmallOrderedHashMap>();
    }
  }

  int hash = key->GetOrCreateHash(isolate)->value();
  int nof = table->NumberOfElements();

  // Read the existing bucket values.
  int bucket = table->HashToBucket(hash);
  int previous_entry = table->HashToFirstEntry(hash);

  // Insert a new entry at the end,
  int new_entry = nof + table->NumberOfDeletedElements();

  table->SetDataEntry(new_entry, SmallOrderedHashMap::kValueIndex, *value);
  table->SetDataEntry(new_entry, SmallOrderedHashMap::kKeyIndex, *key);
  table->SetFirstEntry(bucket, new_entry);
  table->SetNextEntry(new_entry, previous_entry);

  // and update book keeping.
  table->SetNumberOfElements(nof + 1);

  return table;
}

template <class Derived>
bool SmallOrderedHashTable<Derived>::HasKey(Isolate* isolate,
                                            Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  return FindEntry(isolate, *key) != kNotFound;
}

template <class Derived>
bool SmallOrderedHashTable<Derived>::Delete(Isolate* isolate, Derived* table,
                                            Object* key) {
  DisallowHeapAllocation no_gc;
  int entry = table->FindEntry(isolate, key);
  if (entry == kNotFound) return false;

  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();

  Object* hole = ReadOnlyRoots(isolate).the_hole_value();
  for (int j = 0; j < Derived::kEntrySize; j++) {
    table->SetDataEntry(entry, j, hole);
  }

  table->SetNumberOfElements(nof - 1);
  table->SetNumberOfDeletedElements(nod + 1);

  return true;
}

template <class Derived>
Handle<Derived> SmallOrderedHashTable<Derived>::Rehash(Isolate* isolate,
                                                       Handle<Derived> table,
                                                       int new_capacity) {
  DCHECK_GE(kMaxCapacity, new_capacity);

  Handle<Derived> new_table = SmallOrderedHashTable<Derived>::Allocate(
      isolate, new_capacity, Heap::InNewSpace(*table) ? NOT_TENURED : TENURED);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int new_entry = 0;

  {
    DisallowHeapAllocation no_gc;
    for (int old_entry = 0; old_entry < (nof + nod); ++old_entry) {
      Object* key = table->KeyAt(old_entry);
      if (key->IsTheHole(isolate)) continue;

      int hash = Smi::ToInt(key->GetHash());
      int bucket = new_table->HashToBucket(hash);
      int chain = new_table->GetFirstEntry(bucket);

      new_table->SetFirstEntry(bucket, new_entry);
      new_table->SetNextEntry(new_entry, chain);

      for (int i = 0; i < Derived::kEntrySize; ++i) {
        Object* value = table->GetDataEntry(old_entry, i);
        new_table->SetDataEntry(new_entry, i, value);
      }

      ++new_entry;
    }

    new_table->SetNumberOfElements(nof);
  }
  return new_table;
}

template <class Derived>
MaybeHandle<Derived> SmallOrderedHashTable<Derived>::Grow(
    Isolate* isolate, Handle<Derived> table) {
  int capacity = table->Capacity();
  int new_capacity = capacity;

  // Don't need to grow if we can simply clear out deleted entries instead.
  // TODO(gsathya): Compact in place, instead of allocating a new table.
  if (table->NumberOfDeletedElements() < (capacity >> 1)) {
    new_capacity = capacity << 1;

    // The max capacity of our table is 254. We special case for 256 to
    // account for our growth strategy, otherwise we would only fill up
    // to 128 entries in our table.
    if (new_capacity == kGrowthHack) {
      new_capacity = kMaxCapacity;
    }

    // We need to migrate to a bigger hash table.
    if (new_capacity > kMaxCapacity) {
      return MaybeHandle<Derived>();
    }
  }

  return Rehash(isolate, table, new_capacity);
}

template bool SmallOrderedHashTable<SmallOrderedHashSet>::HasKey(
    Isolate* isolate, Handle<Object> key);
template Handle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Rehash(
    Isolate* isolate, Handle<SmallOrderedHashSet> table, int new_capacity);
template MaybeHandle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Grow(
    Isolate* isolate, Handle<SmallOrderedHashSet> table);
template void SmallOrderedHashTable<SmallOrderedHashSet>::Initialize(
    Isolate* isolate, int capacity);

template bool SmallOrderedHashTable<SmallOrderedHashMap>::HasKey(
    Isolate* isolate, Handle<Object> key);
template Handle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Rehash(
    Isolate* isolate, Handle<SmallOrderedHashMap> table, int new_capacity);
template MaybeHandle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Grow(
    Isolate* isolate, Handle<SmallOrderedHashMap> table);
template void SmallOrderedHashTable<SmallOrderedHashMap>::Initialize(
    Isolate* isolate, int capacity);

template bool SmallOrderedHashTable<SmallOrderedHashMap>::Delete(
    Isolate* isolate, SmallOrderedHashMap* table, Object* key);
template bool SmallOrderedHashTable<SmallOrderedHashSet>::Delete(
    Isolate* isolate, SmallOrderedHashSet* table, Object* key);

template <class SmallTable, class LargeTable>
Handle<HeapObject> OrderedHashTableHandler<SmallTable, LargeTable>::Allocate(
    Isolate* isolate, int capacity) {
  if (capacity < SmallTable::kMaxCapacity) {
    return SmallTable::Allocate(isolate, capacity);
  }

  return LargeTable::Allocate(isolate, capacity);
}

template Handle<HeapObject>
OrderedHashTableHandler<SmallOrderedHashSet, OrderedHashSet>::Allocate(
    Isolate* isolate, int capacity);
template Handle<HeapObject>
OrderedHashTableHandler<SmallOrderedHashMap, OrderedHashMap>::Allocate(
    Isolate* isolate, int capacity);

template <class SmallTable, class LargeTable>
bool OrderedHashTableHandler<SmallTable, LargeTable>::Delete(
    Handle<HeapObject> table, Handle<Object> key) {
  if (SmallTable::Is(table)) {
    return SmallTable::Delete(Handle<SmallTable>::cast(table), key);
  }

  DCHECK(LargeTable::Is(table));
  // Note: Once we migrate to the a big hash table, we never migrate
  // down to a smaller hash table.
  return LargeTable::Delete(Handle<LargeTable>::cast(table), key);
}

template <class SmallTable, class LargeTable>
bool OrderedHashTableHandler<SmallTable, LargeTable>::HasKey(
    Isolate* isolate, Handle<HeapObject> table, Handle<Object> key) {
  if (SmallTable::Is(table)) {
    return Handle<SmallTable>::cast(table)->HasKey(isolate, key);
  }

  DCHECK(LargeTable::Is(table));
  return LargeTable::HasKey(isolate, LargeTable::cast(*table), *key);
}

template bool
OrderedHashTableHandler<SmallOrderedHashSet, OrderedHashSet>::HasKey(
    Isolate* isolate, Handle<HeapObject> table, Handle<Object> key);
template bool
OrderedHashTableHandler<SmallOrderedHashMap, OrderedHashMap>::HasKey(
    Isolate* isolate, Handle<HeapObject> table, Handle<Object> key);

Handle<OrderedHashMap> OrderedHashMapHandler::AdjustRepresentation(
    Isolate* isolate, Handle<SmallOrderedHashMap> table) {
  Handle<OrderedHashMap> new_table =
      OrderedHashMap::Allocate(isolate, OrderedHashTableMinSize);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();

  // TODO(gsathya): Optimize the lookup to not re calc offsets. Also,
  // unhandlify this code as we preallocate the new backing store with
  // the proper capacity.
  for (int entry = 0; entry < (nof + nod); ++entry) {
    Handle<Object> key = handle(table->KeyAt(entry), isolate);
    if (key->IsTheHole(isolate)) continue;
    Handle<Object> value = handle(
        table->GetDataEntry(entry, SmallOrderedHashMap::kValueIndex), isolate);
    new_table = OrderedHashMap::Add(isolate, new_table, key, value);
  }

  return new_table;
}

Handle<OrderedHashSet> OrderedHashSetHandler::AdjustRepresentation(
    Isolate* isolate, Handle<SmallOrderedHashSet> table) {
  Handle<OrderedHashSet> new_table =
      OrderedHashSet::Allocate(isolate, OrderedHashTableMinSize);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();

  // TODO(gsathya): Optimize the lookup to not re calc offsets. Also,
  // unhandlify this code as we preallocate the new backing store with
  // the proper capacity.
  for (int entry = 0; entry < (nof + nod); ++entry) {
    Handle<Object> key = handle(table->KeyAt(entry), isolate);
    if (key->IsTheHole(isolate)) continue;
    new_table = OrderedHashSet::Add(isolate, new_table, key);
  }

  return new_table;
}

Handle<HeapObject> OrderedHashMapHandler::Add(Isolate* isolate,
                                              Handle<HeapObject> table,
                                              Handle<Object> key,
                                              Handle<Object> value) {
  if (table->IsSmallOrderedHashMap()) {
    Handle<SmallOrderedHashMap> small_map =
        Handle<SmallOrderedHashMap>::cast(table);
    MaybeHandle<SmallOrderedHashMap> new_map =
        SmallOrderedHashMap::Add(isolate, small_map, key, value);
    if (!new_map.is_null()) return new_map.ToHandleChecked();

    // We couldn't add to the small table, let's migrate to the
    // big table.
    table = OrderedHashMapHandler::AdjustRepresentation(isolate, small_map);
  }

  DCHECK(table->IsOrderedHashMap());
  return OrderedHashMap::Add(isolate, Handle<OrderedHashMap>::cast(table), key,
                             value);
}

Handle<HeapObject> OrderedHashSetHandler::Add(Isolate* isolate,
                                              Handle<HeapObject> table,
                                              Handle<Object> key) {
  if (table->IsSmallOrderedHashSet()) {
    Handle<SmallOrderedHashSet> small_set =
        Handle<SmallOrderedHashSet>::cast(table);
    MaybeHandle<SmallOrderedHashSet> new_set =
        SmallOrderedHashSet::Add(isolate, small_set, key);
    if (!new_set.is_null()) return new_set.ToHandleChecked();

    // We couldn't add to the small table, let's migrate to the
    // big table.
    table = OrderedHashSetHandler::AdjustRepresentation(isolate, small_set);
  }

  DCHECK(table->IsOrderedHashSet());
  return OrderedHashSet::Add(isolate, Handle<OrderedHashSet>::cast(table), key);
}

template <class Derived, class TableType>
void OrderedHashTableIterator<Derived, TableType>::Transition() {
  DisallowHeapAllocation no_allocation;
  TableType* table = TableType::cast(this->table());
  if (!table->IsObsolete()) return;

  int index = Smi::ToInt(this->index());
  while (table->IsObsolete()) {
    TableType* next_table = table->NextTable();

    if (index > 0) {
      int nod = table->NumberOfDeletedElements();

      if (nod == TableType::kClearedTableSentinel) {
        index = 0;
      } else {
        int old_index = index;
        for (int i = 0; i < nod; ++i) {
          int removed_index = table->RemovedIndexAt(i);
          if (removed_index >= old_index) break;
          --index;
        }
      }
    }

    table = next_table;
  }

  set_table(table);
  set_index(Smi::FromInt(index));
}

template <class Derived, class TableType>
bool OrderedHashTableIterator<Derived, TableType>::HasMore() {
  DisallowHeapAllocation no_allocation;
  ReadOnlyRoots ro_roots = GetReadOnlyRoots();

  Transition();

  TableType* table = TableType::cast(this->table());
  int index = Smi::ToInt(this->index());
  int used_capacity = table->UsedCapacity();

  while (index < used_capacity && table->KeyAt(index)->IsTheHole(ro_roots)) {
    index++;
  }

  set_index(Smi::FromInt(index));

  if (index < used_capacity) return true;

  set_table(TableType::GetEmpty(ro_roots));
  return false;
}

template bool
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::HasMore();

template void
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::MoveNext();

template Object*
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::CurrentKey();

template void
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::Transition();

template bool
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::HasMore();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::MoveNext();

template Object*
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::CurrentKey();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::Transition();

}  // namespace internal
}  // namespace v8
