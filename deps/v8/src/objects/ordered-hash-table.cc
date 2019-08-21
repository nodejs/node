// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/ordered-hash-table.h"

#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/ordered-hash-table-inl.h"

namespace v8 {
namespace internal {

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Allocate(
    Isolate* isolate, int capacity, AllocationType allocation) {
  // Capacity must be a power of two, since we depend on being able
  // to divide and multiple by 2 (kLoadFactor) to derive capacity
  // from number of buckets. If we decide to change kLoadFactor
  // to something other than 2, capacity should be stored as another
  // field of this object.
  capacity = base::bits::RoundUpToPowerOfTwo32(Max(kMinCapacity, capacity));
  if (capacity > MaxCapacity()) {
    isolate->heap()->FatalProcessOutOfMemory("invalid table size");
  }
  int num_buckets = capacity / kLoadFactor;
  Handle<FixedArray> backing_store = isolate->factory()->NewFixedArrayWithMap(
      Derived::GetMapRootIndex(),
      HashTableStartIndex() + num_buckets + (capacity * kEntrySize),
      allocation);
  Handle<Derived> table = Handle<Derived>::cast(backing_store);
  for (int i = 0; i < num_buckets; ++i) {
    table->set(HashTableStartIndex() + i, Smi::FromInt(kNotFound));
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
  return Derived::Rehash(isolate, table,
                         (nod < (capacity >> 1)) ? capacity << 1 : capacity);
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Shrink(
    Isolate* isolate, Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int capacity = table->Capacity();
  if (nof >= (capacity >> 2)) return table;
  return Derived::Rehash(isolate, table, capacity / 2);
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Clear(
    Isolate* isolate, Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(isolate, kMinCapacity,
               Heap::InYoungGeneration(*table) ? AllocationType::kYoung
                                               : AllocationType::kOld);

  table->SetNextTable(*new_table);
  table->SetNumberOfDeletedElements(kClearedTableSentinel);

  return new_table;
}

template <class Derived, int entrysize>
bool OrderedHashTable<Derived, entrysize>::HasKey(Isolate* isolate,
                                                  Derived table, Object key) {
  DCHECK_IMPLIES(entrysize == 1, table.IsOrderedHashSet());
  DCHECK_IMPLIES(entrysize == 2, table.IsOrderedHashMap());
  DisallowHeapAllocation no_gc;
  int entry = table.FindEntry(isolate, key);
  return entry != kNotFound;
}

template <class Derived, int entrysize>
int OrderedHashTable<Derived, entrysize>::FindEntry(Isolate* isolate,
                                                    Object key) {
  int entry;
  // This special cases for Smi, so that we avoid the HandleScope
  // creation below.
  if (key.IsSmi()) {
    uint32_t hash = ComputeUnseededHash(Smi::ToInt(key));
    entry = HashToEntry(hash & Smi::kMaxValue);
  } else {
    HandleScope scope(isolate);
    Object hash = key.GetHash();
    // If the object does not have an identity hash, it was never used as a key
    if (hash.IsUndefined(isolate)) return kNotFound;
    entry = HashToEntry(Smi::ToInt(hash));
  }

  // Walk the chain in the bucket to find the key.
  while (entry != kNotFound) {
    Object candidate_key = KeyAt(entry);
    if (candidate_key.SameValueZero(key)) break;
    entry = NextChainEntry(entry);
  }

  return entry;
}

Handle<OrderedHashSet> OrderedHashSet::Add(Isolate* isolate,
                                           Handle<OrderedHashSet> table,
                                           Handle<Object> key) {
  int hash = key->GetOrCreateHash(isolate).value();
  int entry = table->HashToEntry(hash);
  // Walk the chain of the bucket and try finding the key.
  while (entry != kNotFound) {
    Object candidate_key = table->KeyAt(entry);
    // Do not add if we have the key already
    if (candidate_key.SameValueZero(*key)) return table;
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
  table->set(HashTableStartIndex() + bucket, Smi::FromInt(new_entry));
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
    int index = HashTableStartIndex() + nof_buckets + (i * kEntrySize);
    Object key = table->get(index);
    if (convert == GetKeysConversion::kConvertToString) {
      uint32_t index_value;
      if (key.ToArrayIndex(&index_value)) {
        // Avoid trashing the Number2String cache if indices get very large.
        bool use_cache = i < kMaxStringTableEntries;
        key = *isolate->factory()->Uint32ToString(index_value, use_cache);
      } else {
        CHECK(key.IsName());
      }
    }
    result->set(i, key);
  }
  return FixedArray::ShrinkOrEmpty(isolate, result, length);
}

HeapObject OrderedHashSet::GetEmpty(ReadOnlyRoots ro_roots) {
  return ro_roots.empty_ordered_hash_set();
}

HeapObject OrderedHashMap::GetEmpty(ReadOnlyRoots ro_roots) {
  return ro_roots.empty_ordered_hash_map();
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Rehash(
    Isolate* isolate, Handle<Derived> table, int new_capacity) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Derived::Allocate(isolate, new_capacity,
                        Heap::InYoungGeneration(*table) ? AllocationType::kYoung
                                                        : AllocationType::kOld);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int new_buckets = new_table->NumberOfBuckets();
  int new_entry = 0;
  int removed_holes_index = 0;

  DisallowHeapAllocation no_gc;
  for (int old_entry = 0; old_entry < (nof + nod); ++old_entry) {
    Object key = table->KeyAt(old_entry);
    if (key.IsTheHole(isolate)) {
      table->SetRemovedIndexAt(removed_holes_index++, old_entry);
      continue;
    }

    Object hash = key.GetHash();
    int bucket = Smi::ToInt(hash) & (new_buckets - 1);
    Object chain_entry = new_table->get(HashTableStartIndex() + bucket);
    new_table->set(HashTableStartIndex() + bucket, Smi::FromInt(new_entry));
    int new_index = new_table->EntryToIndex(new_entry);
    int old_index = table->EntryToIndex(old_entry);
    for (int i = 0; i < entrysize; ++i) {
      Object value = table->get(old_index + i);
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

Handle<OrderedHashSet> OrderedHashSet::Rehash(Isolate* isolate,
                                              Handle<OrderedHashSet> table,
                                              int new_capacity) {
  return OrderedHashTable<OrderedHashSet, 1>::Rehash(isolate, table,
                                                     new_capacity);
}

Handle<OrderedHashMap> OrderedHashMap::Rehash(Isolate* isolate,
                                              Handle<OrderedHashMap> table,
                                              int new_capacity) {
  return OrderedHashTable<OrderedHashMap, 2>::Rehash(isolate, table,
                                                     new_capacity);
}

Handle<OrderedNameDictionary> OrderedNameDictionary::Rehash(
    Isolate* isolate, Handle<OrderedNameDictionary> table, int new_capacity) {
  Handle<OrderedNameDictionary> new_table =
      OrderedHashTable<OrderedNameDictionary, 3>::Rehash(isolate, table,
                                                         new_capacity);
  new_table->SetHash(table->Hash());
  return new_table;
}

template <class Derived, int entrysize>
bool OrderedHashTable<Derived, entrysize>::Delete(Isolate* isolate,
                                                  Derived table, Object key) {
  DisallowHeapAllocation no_gc;
  int entry = table.FindEntry(isolate, key);
  if (entry == kNotFound) return false;

  int nof = table.NumberOfElements();
  int nod = table.NumberOfDeletedElements();
  int index = table.EntryToIndex(entry);

  Object hole = ReadOnlyRoots(isolate).the_hole_value();
  for (int i = 0; i < entrysize; ++i) {
    table.set(index + i, hole);
  }

  table.SetNumberOfElements(nof - 1);
  table.SetNumberOfDeletedElements(nod + 1);

  return true;
}

Address OrderedHashMap::GetHash(Isolate* isolate, Address raw_key) {
  DisallowHeapAllocation no_gc;
  Object key(raw_key);
  Object hash = key.GetHash();
  // If the object does not have an identity hash, it was never used as a key
  if (hash.IsUndefined(isolate)) return Smi::FromInt(-1).ptr();
  DCHECK(hash.IsSmi());
  DCHECK_GE(Smi::cast(hash).value(), 0);
  return hash.ptr();
}

Handle<OrderedHashMap> OrderedHashMap::Add(Isolate* isolate,
                                           Handle<OrderedHashMap> table,
                                           Handle<Object> key,
                                           Handle<Object> value) {
  int hash = key->GetOrCreateHash(isolate).value();
  int entry = table->HashToEntry(hash);
  // Walk the chain of the bucket and try finding the key.
  {
    DisallowHeapAllocation no_gc;
    Object raw_key = *key;
    while (entry != kNotFound) {
      Object candidate_key = table->KeyAt(entry);
      // Do not add if we have the key already
      if (candidate_key.SameValueZero(raw_key)) return table;
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
  table->set(HashTableStartIndex() + bucket, Smi::FromInt(new_entry));
  table->SetNumberOfElements(nof + 1);
  return table;
}

template <>
V8_EXPORT_PRIVATE int OrderedHashTable<OrderedNameDictionary, 3>::FindEntry(
    Isolate* isolate, Object key) {
  DisallowHeapAllocation no_gc;

  DCHECK(key.IsUniqueName());
  Name raw_key = Name::cast(key);

  int entry = HashToEntry(raw_key.Hash());
  while (entry != kNotFound) {
    Object candidate_key = KeyAt(entry);
    DCHECK(candidate_key.IsTheHole() ||
           Name::cast(candidate_key).IsUniqueName());
    if (candidate_key == raw_key) return entry;

    // TODO(gsathya): This is loading the bucket count from the hash
    // table for every iteration. This should be peeled out of the
    // loop.
    entry = NextChainEntry(entry);
  }

  return kNotFound;
}

Handle<OrderedNameDictionary> OrderedNameDictionary::Add(
    Isolate* isolate, Handle<OrderedNameDictionary> table, Handle<Name> key,
    Handle<Object> value, PropertyDetails details) {
  DCHECK_EQ(kNotFound, table->FindEntry(isolate, *key));

  table = OrderedNameDictionary::EnsureGrowable(isolate, table);
  // Read the existing bucket values.
  int hash = key->Hash();
  int bucket = table->HashToBucket(hash);
  int previous_entry = table->HashToEntry(hash);
  int nof = table->NumberOfElements();
  // Insert a new entry at the end,
  int new_entry = nof + table->NumberOfDeletedElements();
  int new_index = table->EntryToIndex(new_entry);
  table->set(new_index, *key);
  table->set(new_index + kValueOffset, *value);

  // TODO(gsathya): Optimize how PropertyDetails are stored in this
  // dictionary to save memory (by reusing padding?) and performance
  // (by not doing the Smi conversion).
  table->set(new_index + kPropertyDetailsOffset, details.AsSmi());

  table->set(new_index + kChainOffset, Smi::FromInt(previous_entry));
  // and point the bucket to the new entry.
  table->set(HashTableStartIndex() + bucket, Smi::FromInt(new_entry));
  table->SetNumberOfElements(nof + 1);
  return table;
}

void OrderedNameDictionary::SetEntry(Isolate* isolate, int entry, Object key,
                                     Object value, PropertyDetails details) {
  DisallowHeapAllocation gc;
  DCHECK_IMPLIES(!key.IsName(), key.IsTheHole(isolate));
  DisallowHeapAllocation no_gc;
  int index = EntryToIndex(entry);
  this->set(index, key);
  this->set(index + kValueOffset, value);

  // TODO(gsathya): Optimize how PropertyDetails are stored in this
  // dictionary to save memory (by reusing padding?) and performance
  // (by not doing the Smi conversion).
  this->set(index + kPropertyDetailsOffset, details.AsSmi());
}

Handle<OrderedNameDictionary> OrderedNameDictionary::DeleteEntry(
    Isolate* isolate, Handle<OrderedNameDictionary> table, int entry) {
  DCHECK_NE(entry, kNotFound);

  Object hole = ReadOnlyRoots(isolate).the_hole_value();
  PropertyDetails details = PropertyDetails::Empty();
  table->SetEntry(isolate, entry, hole, hole, details);

  int nof = table->NumberOfElements();
  table->SetNumberOfElements(nof - 1);
  int nod = table->NumberOfDeletedElements();
  table->SetNumberOfDeletedElements(nod + 1);

  return Shrink(isolate, table);
}

Handle<OrderedHashSet> OrderedHashSet::Allocate(Isolate* isolate, int capacity,
                                                AllocationType allocation) {
  return OrderedHashTable<OrderedHashSet, 1>::Allocate(isolate, capacity,
                                                       allocation);
}

Handle<OrderedHashMap> OrderedHashMap::Allocate(Isolate* isolate, int capacity,
                                                AllocationType allocation) {
  return OrderedHashTable<OrderedHashMap, 2>::Allocate(isolate, capacity,
                                                       allocation);
}

Handle<OrderedNameDictionary> OrderedNameDictionary::Allocate(
    Isolate* isolate, int capacity, AllocationType allocation) {
  Handle<OrderedNameDictionary> table =
      OrderedHashTable<OrderedNameDictionary, 3>::Allocate(isolate, capacity,
                                                           allocation);
  table->SetHash(PropertyArray::kNoHashSentinel);
  return table;
}

template V8_EXPORT_PRIVATE Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, 1>::EnsureGrowable(
    Isolate* isolate, Handle<OrderedHashSet> table);

template V8_EXPORT_PRIVATE Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, 1>::Shrink(Isolate* isolate,
                                            Handle<OrderedHashSet> table);

template V8_EXPORT_PRIVATE Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, 1>::Clear(Isolate* isolate,
                                           Handle<OrderedHashSet> table);

template V8_EXPORT_PRIVATE bool OrderedHashTable<OrderedHashSet, 1>::HasKey(
    Isolate* isolate, OrderedHashSet table, Object key);

template V8_EXPORT_PRIVATE bool OrderedHashTable<OrderedHashSet, 1>::Delete(
    Isolate* isolate, OrderedHashSet table, Object key);

template V8_EXPORT_PRIVATE int OrderedHashTable<OrderedHashSet, 1>::FindEntry(
    Isolate* isolate, Object key);

template V8_EXPORT_PRIVATE Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, 2>::EnsureGrowable(
    Isolate* isolate, Handle<OrderedHashMap> table);

template V8_EXPORT_PRIVATE Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, 2>::Shrink(Isolate* isolate,
                                            Handle<OrderedHashMap> table);

template V8_EXPORT_PRIVATE Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, 2>::Clear(Isolate* isolate,
                                           Handle<OrderedHashMap> table);

template V8_EXPORT_PRIVATE bool OrderedHashTable<OrderedHashMap, 2>::HasKey(
    Isolate* isolate, OrderedHashMap table, Object key);

template V8_EXPORT_PRIVATE bool OrderedHashTable<OrderedHashMap, 2>::Delete(
    Isolate* isolate, OrderedHashMap table, Object key);

template V8_EXPORT_PRIVATE int OrderedHashTable<OrderedHashMap, 2>::FindEntry(
    Isolate* isolate, Object key);

template Handle<OrderedNameDictionary>
OrderedHashTable<OrderedNameDictionary, 3>::Shrink(
    Isolate* isolate, Handle<OrderedNameDictionary> table);

template Handle<OrderedNameDictionary>
OrderedHashTable<OrderedNameDictionary, 3>::EnsureGrowable(
    Isolate* isolate, Handle<OrderedNameDictionary> table);

template <>
Handle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Allocate(
    Isolate* isolate, int capacity, AllocationType allocation) {
  return isolate->factory()->NewSmallOrderedHashSet(capacity, allocation);
}

template <>
Handle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Allocate(
    Isolate* isolate, int capacity, AllocationType allocation) {
  return isolate->factory()->NewSmallOrderedHashMap(capacity, allocation);
}

template <>
Handle<SmallOrderedNameDictionary>
SmallOrderedHashTable<SmallOrderedNameDictionary>::Allocate(
    Isolate* isolate, int capacity, AllocationType allocation) {
  return isolate->factory()->NewSmallOrderedNameDictionary(capacity,
                                                           allocation);
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
  memset(reinterpret_cast<void*>(field_address(PaddingOffset())), 0,
         PaddingSize());

  Address hashtable_start = GetHashTableStartAddress(capacity);
  memset(reinterpret_cast<byte*>(hashtable_start), kNotFound,
         num_buckets + num_chains);

  if (Heap::InYoungGeneration(*this)) {
    MemsetTagged(RawField(DataTableStartOffset()),
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

  int hash = key->GetOrCreateHash(isolate).value();
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

bool SmallOrderedHashSet::Delete(Isolate* isolate, SmallOrderedHashSet table,
                                 Object key) {
  return SmallOrderedHashTable<SmallOrderedHashSet>::Delete(isolate, table,
                                                            key);
}

bool SmallOrderedHashSet::HasKey(Isolate* isolate, Handle<Object> key) {
  return SmallOrderedHashTable<SmallOrderedHashSet>::HasKey(isolate, key);
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

  int hash = key->GetOrCreateHash(isolate).value();
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

bool SmallOrderedHashMap::Delete(Isolate* isolate, SmallOrderedHashMap table,
                                 Object key) {
  return SmallOrderedHashTable<SmallOrderedHashMap>::Delete(isolate, table,
                                                            key);
}

bool SmallOrderedHashMap::HasKey(Isolate* isolate, Handle<Object> key) {
  return SmallOrderedHashTable<SmallOrderedHashMap>::HasKey(isolate, key);
}

template <>
int V8_EXPORT_PRIVATE
SmallOrderedHashTable<SmallOrderedNameDictionary>::FindEntry(Isolate* isolate,
                                                             Object key) {
  DisallowHeapAllocation no_gc;
  DCHECK(key.IsUniqueName());
  Name raw_key = Name::cast(key);

  int entry = HashToFirstEntry(raw_key.Hash());

  // Walk the chain in the bucket to find the key.
  while (entry != kNotFound) {
    Object candidate_key = KeyAt(entry);
    if (candidate_key == key) return entry;
    entry = GetNextEntry(entry);
  }

  return kNotFound;
}

MaybeHandle<SmallOrderedNameDictionary> SmallOrderedNameDictionary::Add(
    Isolate* isolate, Handle<SmallOrderedNameDictionary> table,
    Handle<Name> key, Handle<Object> value, PropertyDetails details) {
  DCHECK_EQ(kNotFound, table->FindEntry(isolate, *key));

  if (table->UsedCapacity() >= table->Capacity()) {
    MaybeHandle<SmallOrderedNameDictionary> new_table =
        SmallOrderedNameDictionary::Grow(isolate, table);
    if (!new_table.ToHandle(&table)) {
      return MaybeHandle<SmallOrderedNameDictionary>();
    }
  }

  int nof = table->NumberOfElements();

  // Read the existing bucket values.
  int hash = key->Hash();
  int bucket = table->HashToBucket(hash);
  int previous_entry = table->HashToFirstEntry(hash);

  // Insert a new entry at the end,
  int new_entry = nof + table->NumberOfDeletedElements();

  table->SetDataEntry(new_entry, SmallOrderedNameDictionary::kValueIndex,
                      *value);
  table->SetDataEntry(new_entry, SmallOrderedNameDictionary::kKeyIndex, *key);

  // TODO(gsathya): PropertyDetails should be stored as part of the
  // data table to save more memory.
  table->SetDataEntry(new_entry,
                      SmallOrderedNameDictionary::kPropertyDetailsIndex,
                      details.AsSmi());
  table->SetFirstEntry(bucket, new_entry);
  table->SetNextEntry(new_entry, previous_entry);

  // and update book keeping.
  table->SetNumberOfElements(nof + 1);

  return table;
}

void SmallOrderedNameDictionary::SetEntry(Isolate* isolate, int entry,
                                          Object key, Object value,
                                          PropertyDetails details) {
  DCHECK_IMPLIES(!key.IsName(), key.IsTheHole(isolate));
  SetDataEntry(entry, SmallOrderedNameDictionary::kValueIndex, value);
  SetDataEntry(entry, SmallOrderedNameDictionary::kKeyIndex, key);

  // TODO(gsathya): PropertyDetails should be stored as part of the
  // data table to save more memory.
  SetDataEntry(entry, SmallOrderedNameDictionary::kPropertyDetailsIndex,
               details.AsSmi());
}

template <class Derived>
bool SmallOrderedHashTable<Derived>::HasKey(Isolate* isolate,
                                            Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  return FindEntry(isolate, *key) != kNotFound;
}

template <class Derived>
bool SmallOrderedHashTable<Derived>::Delete(Isolate* isolate, Derived table,
                                            Object key) {
  DisallowHeapAllocation no_gc;
  int entry = table.FindEntry(isolate, key);
  if (entry == kNotFound) return false;

  int nof = table.NumberOfElements();
  int nod = table.NumberOfDeletedElements();

  Object hole = ReadOnlyRoots(isolate).the_hole_value();
  for (int j = 0; j < Derived::kEntrySize; j++) {
    table.SetDataEntry(entry, j, hole);
  }

  table.SetNumberOfElements(nof - 1);
  table.SetNumberOfDeletedElements(nod + 1);

  return true;
}

Handle<SmallOrderedNameDictionary> SmallOrderedNameDictionary::DeleteEntry(
    Isolate* isolate, Handle<SmallOrderedNameDictionary> table, int entry) {
  DCHECK_NE(entry, kNotFound);
  {
    DisallowHeapAllocation no_gc;
    Object hole = ReadOnlyRoots(isolate).the_hole_value();
    PropertyDetails details = PropertyDetails::Empty();
    table->SetEntry(isolate, entry, hole, hole, details);

    int nof = table->NumberOfElements();
    table->SetNumberOfElements(nof - 1);
    int nod = table->NumberOfDeletedElements();
    table->SetNumberOfDeletedElements(nod + 1);
  }
  return Shrink(isolate, table);
}

template <class Derived>
Handle<Derived> SmallOrderedHashTable<Derived>::Rehash(Isolate* isolate,
                                                       Handle<Derived> table,
                                                       int new_capacity) {
  DCHECK_GE(kMaxCapacity, new_capacity);

  Handle<Derived> new_table = SmallOrderedHashTable<Derived>::Allocate(
      isolate, new_capacity,
      Heap::InYoungGeneration(*table) ? AllocationType::kYoung
                                      : AllocationType::kOld);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int new_entry = 0;

  {
    DisallowHeapAllocation no_gc;
    for (int old_entry = 0; old_entry < (nof + nod); ++old_entry) {
      Object key = table->KeyAt(old_entry);
      if (key.IsTheHole(isolate)) continue;

      int hash = Smi::ToInt(key.GetHash());
      int bucket = new_table->HashToBucket(hash);
      int chain = new_table->GetFirstEntry(bucket);

      new_table->SetFirstEntry(bucket, new_entry);
      new_table->SetNextEntry(new_entry, chain);

      for (int i = 0; i < Derived::kEntrySize; ++i) {
        Object value = table->GetDataEntry(old_entry, i);
        new_table->SetDataEntry(new_entry, i, value);
      }

      ++new_entry;
    }

    new_table->SetNumberOfElements(nof);
  }
  return new_table;
}

Handle<SmallOrderedHashSet> SmallOrderedHashSet::Rehash(
    Isolate* isolate, Handle<SmallOrderedHashSet> table, int new_capacity) {
  return SmallOrderedHashTable<SmallOrderedHashSet>::Rehash(isolate, table,
                                                            new_capacity);
}

Handle<SmallOrderedHashMap> SmallOrderedHashMap::Rehash(
    Isolate* isolate, Handle<SmallOrderedHashMap> table, int new_capacity) {
  return SmallOrderedHashTable<SmallOrderedHashMap>::Rehash(isolate, table,
                                                            new_capacity);
}

Handle<SmallOrderedNameDictionary> SmallOrderedNameDictionary::Rehash(
    Isolate* isolate, Handle<SmallOrderedNameDictionary> table,
    int new_capacity) {
  Handle<SmallOrderedNameDictionary> new_table =
      SmallOrderedHashTable<SmallOrderedNameDictionary>::Rehash(isolate, table,
                                                                new_capacity);
  new_table->SetHash(table->Hash());
  return new_table;
}

template <class Derived>
Handle<Derived> SmallOrderedHashTable<Derived>::Shrink(Isolate* isolate,
                                                       Handle<Derived> table) {
  int nof = table->NumberOfElements();
  int capacity = table->Capacity();
  if (nof >= (capacity >> 2)) return table;
  return Derived::Rehash(isolate, table, capacity / 2);
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

  return Derived::Rehash(isolate, table, new_capacity);
}

template <class Derived>
int SmallOrderedHashTable<Derived>::FindEntry(Isolate* isolate, Object key) {
  DisallowHeapAllocation no_gc;
  Object hash = key.GetHash();

  if (hash.IsUndefined(isolate)) return kNotFound;
  int entry = HashToFirstEntry(Smi::ToInt(hash));

  // Walk the chain in the bucket to find the key.
  while (entry != kNotFound) {
    Object candidate_key = KeyAt(entry);
    if (candidate_key.SameValueZero(key)) return entry;
    entry = GetNextEntry(entry);
  }
  return kNotFound;
}

template bool EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    SmallOrderedHashTable<SmallOrderedHashSet>::HasKey(Isolate* isolate,
                                                       Handle<Object> key);
template V8_EXPORT_PRIVATE Handle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Rehash(
    Isolate* isolate, Handle<SmallOrderedHashSet> table, int new_capacity);
template V8_EXPORT_PRIVATE Handle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Shrink(
    Isolate* isolate, Handle<SmallOrderedHashSet> table);
template V8_EXPORT_PRIVATE MaybeHandle<SmallOrderedHashSet>
SmallOrderedHashTable<SmallOrderedHashSet>::Grow(
    Isolate* isolate, Handle<SmallOrderedHashSet> table);
template V8_EXPORT_PRIVATE void
SmallOrderedHashTable<SmallOrderedHashSet>::Initialize(Isolate* isolate,
                                                       int capacity);
template V8_EXPORT_PRIVATE bool
SmallOrderedHashTable<SmallOrderedHashSet>::Delete(Isolate* isolate,
                                                   SmallOrderedHashSet table,
                                                   Object key);

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) bool SmallOrderedHashTable<
    SmallOrderedHashMap>::HasKey(Isolate* isolate, Handle<Object> key);
template V8_EXPORT_PRIVATE Handle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Rehash(
    Isolate* isolate, Handle<SmallOrderedHashMap> table, int new_capacity);
template V8_EXPORT_PRIVATE Handle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Shrink(
    Isolate* isolate, Handle<SmallOrderedHashMap> table);
template V8_EXPORT_PRIVATE MaybeHandle<SmallOrderedHashMap>
SmallOrderedHashTable<SmallOrderedHashMap>::Grow(
    Isolate* isolate, Handle<SmallOrderedHashMap> table);
template V8_EXPORT_PRIVATE void
SmallOrderedHashTable<SmallOrderedHashMap>::Initialize(Isolate* isolate,
                                                       int capacity);

template V8_EXPORT_PRIVATE bool
SmallOrderedHashTable<SmallOrderedHashMap>::Delete(Isolate* isolate,
                                                   SmallOrderedHashMap table,
                                                   Object key);

template V8_EXPORT_PRIVATE void
SmallOrderedHashTable<SmallOrderedNameDictionary>::Initialize(Isolate* isolate,
                                                              int capacity);
template V8_EXPORT_PRIVATE Handle<SmallOrderedNameDictionary>
SmallOrderedHashTable<SmallOrderedNameDictionary>::Shrink(
    Isolate* isolate, Handle<SmallOrderedNameDictionary> table);

template <class SmallTable, class LargeTable>
Handle<HeapObject> OrderedHashTableHandler<SmallTable, LargeTable>::Allocate(
    Isolate* isolate, int capacity) {
  if (capacity < SmallTable::kMaxCapacity) {
    return SmallTable::Allocate(isolate, capacity);
  }

  return LargeTable::Allocate(isolate, capacity);
}

template V8_EXPORT_PRIVATE Handle<HeapObject>
OrderedHashTableHandler<SmallOrderedHashSet, OrderedHashSet>::Allocate(
    Isolate* isolate, int capacity);
template V8_EXPORT_PRIVATE Handle<HeapObject>
OrderedHashTableHandler<SmallOrderedHashMap, OrderedHashMap>::Allocate(
    Isolate* isolate, int capacity);
template V8_EXPORT_PRIVATE Handle<HeapObject>
OrderedHashTableHandler<SmallOrderedNameDictionary,
                        OrderedNameDictionary>::Allocate(Isolate* isolate,
                                                         int capacity);

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

Handle<OrderedNameDictionary>
OrderedNameDictionaryHandler::AdjustRepresentation(
    Isolate* isolate, Handle<SmallOrderedNameDictionary> table) {
  Handle<OrderedNameDictionary> new_table =
      OrderedNameDictionary::Allocate(isolate, OrderedHashTableMinSize);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();

  // TODO(gsathya): Optimize the lookup to not re calc offsets. Also,
  // unhandlify this code as we preallocate the new backing store with
  // the proper capacity.
  for (int entry = 0; entry < (nof + nod); ++entry) {
    Handle<Name> key(Name::cast(table->KeyAt(entry)), isolate);
    if (key->IsTheHole(isolate)) continue;
    Handle<Object> value(table->ValueAt(entry), isolate);
    PropertyDetails details = table->DetailsAt(entry);
    new_table =
        OrderedNameDictionary::Add(isolate, new_table, key, value, details);
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

Handle<HeapObject> OrderedNameDictionaryHandler::Add(Isolate* isolate,
                                                     Handle<HeapObject> table,
                                                     Handle<Name> key,
                                                     Handle<Object> value,
                                                     PropertyDetails details) {
  if (table->IsSmallOrderedNameDictionary()) {
    Handle<SmallOrderedNameDictionary> small_dict =
        Handle<SmallOrderedNameDictionary>::cast(table);
    MaybeHandle<SmallOrderedNameDictionary> new_dict =
        SmallOrderedNameDictionary::Add(isolate, small_dict, key, value,
                                        details);
    if (!new_dict.is_null()) return new_dict.ToHandleChecked();

    // We couldn't add to the small table, let's migrate to the
    // big table.
    table =
        OrderedNameDictionaryHandler::AdjustRepresentation(isolate, small_dict);
  }

  DCHECK(table->IsOrderedNameDictionary());
  return OrderedNameDictionary::Add(
      isolate, Handle<OrderedNameDictionary>::cast(table), key, value, details);
}

void OrderedNameDictionaryHandler::SetEntry(Isolate* isolate, HeapObject table,
                                            int entry, Object key, Object value,
                                            PropertyDetails details) {
  DisallowHeapAllocation no_gc;
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).SetEntry(isolate, entry, key,
                                                            value, details);
  }

  DCHECK(table.IsOrderedNameDictionary());
  return OrderedNameDictionary::cast(table).SetEntry(isolate, entry, key, value,
                                                     details);
}

int OrderedNameDictionaryHandler::FindEntry(Isolate* isolate, HeapObject table,
                                            Name key) {
  DisallowHeapAllocation no_gc;
  if (table.IsSmallOrderedNameDictionary()) {
    int entry = SmallOrderedNameDictionary::cast(table).FindEntry(isolate, key);
    return entry == SmallOrderedNameDictionary::kNotFound
               ? OrderedNameDictionaryHandler::kNotFound
               : entry;
  }

  DCHECK(table.IsOrderedNameDictionary());
  int entry = OrderedNameDictionary::cast(table).FindEntry(isolate, key);
  return entry == OrderedNameDictionary::kNotFound
             ? OrderedNameDictionaryHandler::kNotFound
             : entry;
}

Object OrderedNameDictionaryHandler::ValueAt(HeapObject table, int entry) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).ValueAt(entry);
  }

  DCHECK(table.IsOrderedNameDictionary());
  return OrderedNameDictionary::cast(table).ValueAt(entry);
}

void OrderedNameDictionaryHandler::ValueAtPut(HeapObject table, int entry,
                                              Object value) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).ValueAtPut(entry, value);
  }

  DCHECK(table.IsOrderedNameDictionary());
  OrderedNameDictionary::cast(table).ValueAtPut(entry, value);
}

PropertyDetails OrderedNameDictionaryHandler::DetailsAt(HeapObject table,
                                                        int entry) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).DetailsAt(entry);
  }

  DCHECK(table.IsOrderedNameDictionary());
  return OrderedNameDictionary::cast(table).DetailsAt(entry);
}

void OrderedNameDictionaryHandler::DetailsAtPut(HeapObject table, int entry,
                                                PropertyDetails details) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).DetailsAtPut(entry, details);
  }

  DCHECK(table.IsOrderedNameDictionary());
  OrderedNameDictionary::cast(table).DetailsAtPut(entry, details);
}

int OrderedNameDictionaryHandler::Hash(HeapObject table) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).Hash();
  }

  DCHECK(table.IsOrderedNameDictionary());
  return OrderedNameDictionary::cast(table).Hash();
}

void OrderedNameDictionaryHandler::SetHash(HeapObject table, int hash) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).SetHash(hash);
  }

  DCHECK(table.IsOrderedNameDictionary());
  OrderedNameDictionary::cast(table).SetHash(hash);
}

Name OrderedNameDictionaryHandler::KeyAt(HeapObject table, int entry) {
  if (table.IsSmallOrderedNameDictionary()) {
    return Name::cast(SmallOrderedNameDictionary::cast(table).KeyAt(entry));
  }

  return Name::cast(OrderedNameDictionary::cast(table).KeyAt(entry));
}

int OrderedNameDictionaryHandler::NumberOfElements(HeapObject table) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).NumberOfElements();
  }

  return OrderedNameDictionary::cast(table).NumberOfElements();
}

int OrderedNameDictionaryHandler::Capacity(HeapObject table) {
  if (table.IsSmallOrderedNameDictionary()) {
    return SmallOrderedNameDictionary::cast(table).Capacity();
  }

  return OrderedNameDictionary::cast(table).Capacity();
}

Handle<HeapObject> OrderedNameDictionaryHandler::Shrink(
    Isolate* isolate, Handle<HeapObject> table) {
  if (table->IsSmallOrderedNameDictionary()) {
    Handle<SmallOrderedNameDictionary> small_dict =
        Handle<SmallOrderedNameDictionary>::cast(table);
    return SmallOrderedNameDictionary::Shrink(isolate, small_dict);
  }

  Handle<OrderedNameDictionary> large_dict =
      Handle<OrderedNameDictionary>::cast(table);
  return OrderedNameDictionary::Shrink(isolate, large_dict);
}

Handle<HeapObject> OrderedNameDictionaryHandler::DeleteEntry(
    Isolate* isolate, Handle<HeapObject> table, int entry) {
  DisallowHeapAllocation no_gc;
  if (table->IsSmallOrderedNameDictionary()) {
    Handle<SmallOrderedNameDictionary> small_dict =
        Handle<SmallOrderedNameDictionary>::cast(table);
    return SmallOrderedNameDictionary::DeleteEntry(isolate, small_dict, entry);
  }

  Handle<OrderedNameDictionary> large_dict =
      Handle<OrderedNameDictionary>::cast(table);
  return OrderedNameDictionary::DeleteEntry(isolate, large_dict, entry);
}

template <class Derived, class TableType>
void OrderedHashTableIterator<Derived, TableType>::Transition() {
  DisallowHeapAllocation no_allocation;
  TableType table = TableType::cast(this->table());
  if (!table.IsObsolete()) return;

  int index = Smi::ToInt(this->index());
  while (table.IsObsolete()) {
    TableType next_table = table.NextTable();

    if (index > 0) {
      int nod = table.NumberOfDeletedElements();

      if (nod == TableType::kClearedTableSentinel) {
        index = 0;
      } else {
        int old_index = index;
        for (int i = 0; i < nod; ++i) {
          int removed_index = table.RemovedIndexAt(i);
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

  TableType table = TableType::cast(this->table());
  int index = Smi::ToInt(this->index());
  int used_capacity = table.UsedCapacity();

  while (index < used_capacity && table.KeyAt(index).IsTheHole(ro_roots)) {
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

template Object
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::CurrentKey();

template void
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::Transition();

template bool
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::HasMore();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::MoveNext();

template Object
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::CurrentKey();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::Transition();

}  // namespace internal
}  // namespace v8
