// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/ordered-hash-table.h"

#include "src/isolate.h"
#include "src/objects-inl.h"
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
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int capacity = table->Capacity();
  if ((nof + nod) < capacity) return table;
  // Don't need to grow if we can simply clear out deleted entries instead.
  // Note that we can't compact in place, though, so we always allocate
  // a new table.
  return Rehash(table, (nod < (capacity >> 1)) ? capacity << 1 : capacity);
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Shrink(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int capacity = table->Capacity();
  if (nof >= (capacity >> 2)) return table;
  return Rehash(table, capacity / 2);
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Clear(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(table->GetIsolate(), kMinCapacity,
               table->GetHeap()->InNewSpace(*table) ? NOT_TENURED : TENURED);

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

Handle<OrderedHashSet> OrderedHashSet::Add(Handle<OrderedHashSet> table,
                                           Handle<Object> key) {
  int hash = key->GetOrCreateHash(table->GetIsolate())->value();
  int entry = table->HashToEntry(hash);
  // Walk the chain of the bucket and try finding the key.
  while (entry != kNotFound) {
    Object* candidate_key = table->KeyAt(entry);
    // Do not add if we have the key already
    if (candidate_key->SameValueZero(*key)) return table;
    entry = table->NextChainEntry(entry);
  }

  table = OrderedHashSet::EnsureGrowable(table);
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
    Handle<OrderedHashSet> table, GetKeysConversion convert) {
  Isolate* isolate = table->GetIsolate();
  int length = table->NumberOfElements();
  int nof_buckets = table->NumberOfBuckets();
  // Convert the dictionary to a linear list.
  Handle<FixedArray> result = Handle<FixedArray>::cast(table);
  // From this point on table is no longer a valid OrderedHashSet.
  result->set_map(isolate->heap()->fixed_array_map());
  for (int i = 0; i < length; i++) {
    int index = kHashTableStartIndex + nof_buckets + (i * kEntrySize);
    Object* key = table->get(index);
    if (convert == GetKeysConversion::kConvertToString) {
      uint32_t index_value;
      if (key->ToArrayIndex(&index_value)) {
        key = *isolate->factory()->Uint32ToString(index_value);
      } else {
        CHECK(key->IsName());
      }
    }
    result->set(i, key);
  }
  result->Shrink(length);
  return result;
}

HeapObject* OrderedHashSet::GetEmpty(Isolate* isolate) {
  return isolate->heap()->empty_ordered_hash_set();
}

HeapObject* OrderedHashMap::GetEmpty(Isolate* isolate) {
  return isolate->heap()->empty_ordered_hash_map();
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Rehash(
    Handle<Derived> table, int new_capacity) {
  Isolate* isolate = table->GetIsolate();
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(isolate, new_capacity,
               isolate->heap()->InNewSpace(*table) ? NOT_TENURED : TENURED);
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

  Object* hole = isolate->heap()->the_hole_value();
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

Handle<OrderedHashMap> OrderedHashMap::Add(Handle<OrderedHashMap> table,
                                           Handle<Object> key,
                                           Handle<Object> value) {
  int hash = key->GetOrCreateHash(table->GetIsolate())->value();
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

  table = OrderedHashMap::EnsureGrowable(table);
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

template Handle<OrderedHashSet> OrderedHashTable<
    OrderedHashSet, 1>::EnsureGrowable(Handle<OrderedHashSet> table);

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Shrink(
    Handle<OrderedHashSet> table);

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Clear(
    Handle<OrderedHashSet> table);

template bool OrderedHashTable<OrderedHashSet, 1>::HasKey(Isolate* isolate,
                                                          OrderedHashSet* table,
                                                          Object* key);

template bool OrderedHashTable<OrderedHashSet, 1>::Delete(Isolate* isolate,
                                                          OrderedHashSet* table,
                                                          Object* key);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashMap> OrderedHashTable<
    OrderedHashMap, 2>::EnsureGrowable(Handle<OrderedHashMap> table);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Shrink(
    Handle<OrderedHashMap> table);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Clear(
    Handle<OrderedHashMap> table);

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

  if (isolate->heap()->InNewSpace(this)) {
    MemsetPointer(RawField(this, kHeaderSize + kDataTableStartOffset),
                  isolate->heap()->the_hole_value(),
                  capacity * Derived::kEntrySize);
  } else {
    for (int i = 0; i < capacity; i++) {
      for (int j = 0; j < Derived::kEntrySize; j++) {
        SetDataEntry(i, j, isolate->heap()->the_hole_value());
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
      DCHECK_EQ(isolate->heap()->the_hole_value(), GetDataEntry(i, j));
    }
  }
#endif  // DEBUG
}

Handle<SmallOrderedHashSet> SmallOrderedHashSet::Add(
    Handle<SmallOrderedHashSet> table, Handle<Object> key) {
  Isolate* isolate = table->GetIsolate();
  if (table->HasKey(isolate, key)) return table;

  if (table->UsedCapacity() >= table->Capacity()) {
    table = SmallOrderedHashSet::Grow(table);
  }

  int hash = key->GetOrCreateHash(table->GetIsolate())->value();
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

Handle<SmallOrderedHashMap> SmallOrderedHashMap::Add(
    Handle<SmallOrderedHashMap> table, Handle<Object> key,
    Handle<Object> value) {
  Isolate* isolate = table->GetIsolate();
  if (table->HasKey(isolate, key)) return table;

  if (table->UsedCapacity() >= table->Capacity()) {
    table = SmallOrderedHashMap::Grow(table);
  }

  int hash = key->GetOrCreateHash(table->GetIsolate())->value();
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

  Object* hole = isolate->heap()->the_hole_value();
  for (int j = 0; j < Derived::kEntrySize; j++) {
    table->SetDataEntry(entry, j, hole);
  }

  table->SetNumberOfElements(nof - 1);
  table->SetNumberOfDeletedElements(nod + 1);

  return true;
}

template <class Derived>
Handle<Derived> SmallOrderedHashTable<Derived>::Rehash(Handle<Derived> table,
                                                       int new_capacity) {
  DCHECK_GE(kMaxCapacity, new_capacity);
  Isolate* isolate = table->GetIsolate();

  Handle<Derived> new_table = SmallOrderedHashTable<Derived>::Allocate(
      isolate, new_capacity,
      isolate->heap()->InNewSpace(*table) ? NOT_TENURED : TENURED);
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
Handle<Derived> SmallOrderedHashTable<Derived>::Grow(Handle<Derived> table) {
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

    // TODO(gsathya): Transition to OrderedHashTable for size > kMaxCapacity.
  }

  return Rehash(table, new_capacity);
}

template bool SmallOrderedHashTable<SmallOrderedHashSet>::HasKey(
    Isolate* isolate, Handle<Object> key);
template Handle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Rehash(
    Handle<SmallOrderedHashSet> table, int new_capacity);
template Handle<SmallOrderedHashSet> SmallOrderedHashTable<
    SmallOrderedHashSet>::Grow(Handle<SmallOrderedHashSet> table);
template void SmallOrderedHashTable<SmallOrderedHashSet>::Initialize(
    Isolate* isolate, int capacity);

template bool SmallOrderedHashTable<SmallOrderedHashMap>::HasKey(
    Isolate* isolate, Handle<Object> key);
template Handle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Rehash(
    Handle<SmallOrderedHashMap> table, int new_capacity);
template Handle<SmallOrderedHashMap> SmallOrderedHashTable<
    SmallOrderedHashMap>::Grow(Handle<SmallOrderedHashMap> table);
template void SmallOrderedHashTable<SmallOrderedHashMap>::Initialize(
    Isolate* isolate, int capacity);

template bool SmallOrderedHashTable<SmallOrderedHashMap>::Delete(
    Isolate* isolate, SmallOrderedHashMap* table, Object* key);
template bool SmallOrderedHashTable<SmallOrderedHashSet>::Delete(
    Isolate* isolate, SmallOrderedHashSet* table, Object* key);

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
  Isolate* isolate = this->GetIsolate();

  Transition();

  TableType* table = TableType::cast(this->table());
  int index = Smi::ToInt(this->index());
  int used_capacity = table->UsedCapacity();

  while (index < used_capacity && table->KeyAt(index)->IsTheHole(isolate)) {
    index++;
  }

  set_index(Smi::FromInt(index));

  if (index < used_capacity) return true;

  set_table(TableType::GetEmpty(isolate));
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
