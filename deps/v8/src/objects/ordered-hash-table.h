// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ORDERED_HASH_TABLE_H_
#define V8_OBJECTS_ORDERED_HASH_TABLE_H_

#include "src/globals.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Non-templatized base class for {OrderedHashTable}s.
// TODO(hash): Unify this with the HashTableBase above.
class OrderedHashTableBase : public FixedArray {
 public:
  static const int kNotFound = -1;
  static const int kMinCapacity = 4;

  static const int kNumberOfElementsIndex = 0;
  // The next table is stored at the same index as the nof elements.
  static const int kNextTableIndex = kNumberOfElementsIndex;
  static const int kNumberOfDeletedElementsIndex = kNumberOfElementsIndex + 1;
  static const int kNumberOfBucketsIndex = kNumberOfDeletedElementsIndex + 1;
  static const int kHashTableStartIndex = kNumberOfBucketsIndex + 1;
  static const int kRemovedHolesIndex = kHashTableStartIndex;

  static constexpr const int kNumberOfElementsOffset =
      FixedArray::OffsetOfElementAt(kNumberOfElementsIndex);
  static constexpr const int kNextTableOffset =
      FixedArray::OffsetOfElementAt(kNextTableIndex);
  static constexpr const int kNumberOfDeletedElementsOffset =
      FixedArray::OffsetOfElementAt(kNumberOfDeletedElementsIndex);
  static constexpr const int kNumberOfBucketsOffset =
      FixedArray::OffsetOfElementAt(kNumberOfBucketsIndex);
  static constexpr const int kHashTableStartOffset =
      FixedArray::OffsetOfElementAt(kHashTableStartIndex);

  static const int kLoadFactor = 2;

  // NumberOfDeletedElements is set to kClearedTableSentinel when
  // the table is cleared, which allows iterator transitions to
  // optimize that case.
  static const int kClearedTableSentinel = -1;
};

// OrderedHashTable is a HashTable with Object keys that preserves
// insertion order. There are Map and Set interfaces (OrderedHashMap
// and OrderedHashTable, below). It is meant to be used by JSMap/JSSet.
//
// Only Object* keys are supported, with Object::SameValueZero() used as the
// equality operator and Object::GetHash() for the hash function.
//
// Based on the "Deterministic Hash Table" as described by Jason Orendorff at
// https://wiki.mozilla.org/User:Jorend/Deterministic_hash_tables
// Originally attributed to Tyler Close.
//
// Memory layout:
//   [0]: element count
//   [1]: deleted element count
//   [2]: bucket count
//   [3..(3 + NumberOfBuckets() - 1)]: "hash table", where each item is an
//                            offset into the data table (see below) where the
//                            first item in this bucket is stored.
//   [3 + NumberOfBuckets()..length]: "data table", an array of length
//                            Capacity() * kEntrySize, where the first entrysize
//                            items are handled by the derived class and the
//                            item at kChainOffset is another entry into the
//                            data table indicating the next entry in this hash
//                            bucket.
//
// When we transition the table to a new version we obsolete it and reuse parts
// of the memory to store information how to transition an iterator to the new
// table:
//
// Memory layout for obsolete table:
//   [0]: bucket count
//   [1]: Next newer table
//   [2]: Number of removed holes or -1 when the table was cleared.
//   [3..(3 + NumberOfRemovedHoles() - 1)]: The indexes of the removed holes.
//   [3 + NumberOfRemovedHoles()..length]: Not used
//
template <class Derived, int entrysize>
class OrderedHashTable : public OrderedHashTableBase {
 public:
  // Returns an OrderedHashTable with a capacity of at least |capacity|.
  static Handle<Derived> Allocate(Isolate* isolate, int capacity,
                                  PretenureFlag pretenure = NOT_TENURED);

  // Returns an OrderedHashTable (possibly |table|) with enough space
  // to add at least one new element.
  static Handle<Derived> EnsureGrowable(Isolate* isolate,
                                        Handle<Derived> table);

  // Returns an OrderedHashTable (possibly |table|) that's shrunken
  // if possible.
  static Handle<Derived> Shrink(Isolate* isolate, Handle<Derived> table);

  // Returns a new empty OrderedHashTable and records the clearing so that
  // existing iterators can be updated.
  static Handle<Derived> Clear(Isolate* isolate, Handle<Derived> table);

  // Returns true if the OrderedHashTable contains the key
  static bool HasKey(Isolate* isolate, Derived* table, Object* key);

  // Returns a true value if the OrderedHashTable contains the key and
  // the key has been deleted. This does not shrink the table.
  static bool Delete(Isolate* isolate, Derived* table, Object* key);

  int NumberOfElements() const {
    return Smi::ToInt(get(kNumberOfElementsIndex));
  }

  int NumberOfDeletedElements() const {
    return Smi::ToInt(get(kNumberOfDeletedElementsIndex));
  }

  // Returns the number of contiguous entries in the data table, starting at 0,
  // that either are real entries or have been deleted.
  int UsedCapacity() const {
    return NumberOfElements() + NumberOfDeletedElements();
  }

  int NumberOfBuckets() const { return Smi::ToInt(get(kNumberOfBucketsIndex)); }

  // Returns an index into |this| for the given entry.
  int EntryToIndex(int entry) {
    return kHashTableStartIndex + NumberOfBuckets() + (entry * kEntrySize);
  }

  int HashToBucket(int hash) { return hash & (NumberOfBuckets() - 1); }

  int HashToEntry(int hash) {
    int bucket = HashToBucket(hash);
    Object* entry = this->get(kHashTableStartIndex + bucket);
    return Smi::ToInt(entry);
  }

  int KeyToFirstEntry(Isolate* isolate, Object* key) {
    // This special cases for Smi, so that we avoid the HandleScope
    // creation below.
    if (key->IsSmi()) {
      uint32_t hash = ComputeUnseededHash(Smi::ToInt(key));
      return HashToEntry(hash & Smi::kMaxValue);
    }
    HandleScope scope(isolate);
    Object* hash = key->GetHash();
    // If the object does not have an identity hash, it was never used as a key
    if (hash->IsUndefined(isolate)) return kNotFound;
    return HashToEntry(Smi::ToInt(hash));
  }

  int FindEntry(Isolate* isolate, Object* key) {
    int entry = KeyToFirstEntry(isolate, key);
    // Walk the chain in the bucket to find the key.
    while (entry != kNotFound) {
      Object* candidate_key = KeyAt(entry);
      if (candidate_key->SameValueZero(key)) break;
      entry = NextChainEntry(entry);
    }

    return entry;
  }

  int NextChainEntry(int entry) {
    Object* next_entry = get(EntryToIndex(entry) + kChainOffset);
    return Smi::ToInt(next_entry);
  }

  // use KeyAt(i)->IsTheHole(isolate) to determine if this is a deleted entry.
  Object* KeyAt(int entry) {
    DCHECK_LT(entry, this->UsedCapacity());
    return get(EntryToIndex(entry));
  }

  bool IsObsolete() { return !get(kNextTableIndex)->IsSmi(); }

  // The next newer table. This is only valid if the table is obsolete.
  Derived* NextTable() { return Derived::cast(get(kNextTableIndex)); }

  // When the table is obsolete we store the indexes of the removed holes.
  int RemovedIndexAt(int index) {
    return Smi::ToInt(get(kRemovedHolesIndex + index));
  }

  static const int kEntrySize = entrysize + 1;
  static const int kChainOffset = entrysize;

  static const int kMaxCapacity =
      (FixedArray::kMaxLength - kHashTableStartIndex) /
      (1 + (kEntrySize * kLoadFactor));

 protected:
  static Handle<Derived> Rehash(Isolate* isolate, Handle<Derived> table,
                                int new_capacity);

  void SetNumberOfBuckets(int num) {
    set(kNumberOfBucketsIndex, Smi::FromInt(num));
  }

  void SetNumberOfElements(int num) {
    set(kNumberOfElementsIndex, Smi::FromInt(num));
  }

  void SetNumberOfDeletedElements(int num) {
    set(kNumberOfDeletedElementsIndex, Smi::FromInt(num));
  }

  // Returns the number elements that can fit into the allocated buffer.
  int Capacity() { return NumberOfBuckets() * kLoadFactor; }

  void SetNextTable(Derived* next_table) { set(kNextTableIndex, next_table); }

  void SetRemovedIndexAt(int index, int removed_index) {
    return set(kRemovedHolesIndex + index, Smi::FromInt(removed_index));
  }
};

class OrderedHashSet : public OrderedHashTable<OrderedHashSet, 1> {
 public:
  DECL_CAST(OrderedHashSet)

  static Handle<OrderedHashSet> Add(Isolate* isolate,
                                    Handle<OrderedHashSet> table,
                                    Handle<Object> value);
  static Handle<FixedArray> ConvertToKeysArray(Isolate* isolate,
                                               Handle<OrderedHashSet> table,
                                               GetKeysConversion convert);
  static HeapObject* GetEmpty(ReadOnlyRoots ro_roots);
  static inline RootIndex GetMapRootIndex();
  static inline bool Is(Handle<HeapObject> table);
};

class OrderedHashMap : public OrderedHashTable<OrderedHashMap, 2> {
 public:
  DECL_CAST(OrderedHashMap)

  // Returns a value if the OrderedHashMap contains the key, otherwise
  // returns undefined.
  static Handle<OrderedHashMap> Add(Isolate* isolate,
                                    Handle<OrderedHashMap> table,
                                    Handle<Object> key, Handle<Object> value);
  Object* ValueAt(int entry);

  static Object* GetHash(Isolate* isolate, Object* key);

  static HeapObject* GetEmpty(ReadOnlyRoots ro_roots);
  static inline RootIndex GetMapRootIndex();
  static inline bool Is(Handle<HeapObject> table);

  static const int kValueOffset = 1;
};

// This is similar to the OrderedHashTable, except for the memory
// layout where we use byte instead of Smi. The max capacity of this
// is only 254, we transition to an OrderedHashTable beyond that
// limit.
//
// Each bucket and chain value is a byte long. The padding exists so
// that the DataTable entries start aligned. A bucket or chain value
// of 255 is used to denote an unknown entry.
//
// Memory layout: [ Header ]  [ Padding ] [ DataTable ] [ HashTable ] [ Chains ]
//
// The index are represented as bytes, on a 64 bit machine with
// kEntrySize = 1, capacity = 4 and entries = 2:
//
// [ Header ]  :
//    [0] : Number of elements
//    [1] : Number of deleted elements
//    [2] : Number of buckets
//
// [ Padding ] :
//    [3 .. 7] : Padding
//
// [ DataTable ] :
//    [8  .. 15] : Entry 1
//    [16 .. 23] : Entry 2
//    [24 .. 31] : empty
//    [32 .. 39] : empty
//
// [ HashTable ] :
//    [40] : First chain-link for bucket 1
//    [41] : empty
//
// [ Chains ] :
//    [42] : Next chain link for bucket 1
//    [43] : empty
//    [44] : empty
//    [45] : empty
//
template <class Derived>
class SmallOrderedHashTable : public HeapObject {
 public:
  // Offset points to a relative location in the table
  typedef int Offset;

  // ByteIndex points to a index in the table that needs to be
  // converted to an Offset.
  typedef int ByteIndex;

  void Initialize(Isolate* isolate, int capacity);

  static Handle<Derived> Allocate(Isolate* isolate, int capacity,
                                  PretenureFlag pretenure = NOT_TENURED);

  // Returns a true if the OrderedHashTable contains the key
  bool HasKey(Isolate* isolate, Handle<Object> key);

  // Returns a true value if the table contains the key and
  // the key has been deleted. This does not shrink the table.
  static bool Delete(Isolate* isolate, Derived* table, Object* key);

  // Returns an SmallOrderedHashTable (possibly |table|) with enough
  // space to add at least one new element. Returns empty handle if
  // we've already reached MaxCapacity.
  static MaybeHandle<Derived> Grow(Isolate* isolate, Handle<Derived> table);

  static Handle<Derived> Rehash(Isolate* isolate, Handle<Derived> table,
                                int new_capacity);

  // Iterates only fields in the DataTable.
  class BodyDescriptor;

  // Returns total size in bytes required for a table of given
  // capacity.
  static int SizeFor(int capacity) {
    DCHECK_GE(capacity, kMinCapacity);
    DCHECK_LE(capacity, kMaxCapacity);

    int data_table_size = DataTableSizeFor(capacity);
    int hash_table_size = capacity / kLoadFactor;
    int chain_table_size = capacity;
    int total_size = kDataTableStartOffset + data_table_size + hash_table_size +
                     chain_table_size;

    return RoundUp(total_size, kPointerSize);
  }

  // Returns the number elements that can fit into the allocated table.
  int Capacity() const {
    int capacity = NumberOfBuckets() * kLoadFactor;
    DCHECK_GE(capacity, kMinCapacity);
    DCHECK_LE(capacity, kMaxCapacity);

    return capacity;
  }

  // Returns the number elements that are present in the table.
  int NumberOfElements() const {
    int nof_elements = getByte(kNumberOfElementsOffset, 0);
    DCHECK_LE(nof_elements, Capacity());

    return nof_elements;
  }

  int NumberOfDeletedElements() const {
    int nof_deleted_elements = getByte(kNumberOfDeletedElementsOffset, 0);
    DCHECK_LE(nof_deleted_elements, Capacity());

    return nof_deleted_elements;
  }

  int NumberOfBuckets() const { return getByte(kNumberOfBucketsOffset, 0); }

  DECL_VERIFIER(SmallOrderedHashTable)

  static const int kMinCapacity = 4;
  static const byte kNotFound = 0xFF;

  // We use the value 255 to indicate kNotFound for chain and bucket
  // values, which means that this value can't be used a valid
  // index.
  static const int kMaxCapacity = 254;
  STATIC_ASSERT(kMaxCapacity < kNotFound);

  // The load factor is used to derive the number of buckets from
  // capacity during Allocation. We also depend on this to calaculate
  // the capacity from number of buckets after allocation. If we
  // decide to change kLoadFactor to something other than 2, capacity
  // should be stored as another field of this object.
  static const int kLoadFactor = 2;

  // Our growth strategy involves doubling the capacity until we reach
  // kMaxCapacity, but since the kMaxCapacity is always less than 256,
  // we will never fully utilize this table. We special case for 256,
  // by changing the new capacity to be kMaxCapacity in
  // SmallOrderedHashTable::Grow.
  static const int kGrowthHack = 256;

 protected:
  void SetDataEntry(int entry, int relative_index, Object* value);

  // TODO(gsathya): Calculate all the various possible values for this
  // at compile time since capacity can only be 4 different values.
  Offset GetBucketsStartOffset() const {
    int capacity = Capacity();
    int data_table_size = DataTableSizeFor(capacity);
    return kDataTableStartOffset + data_table_size;
  }

  Address GetHashTableStartAddress(int capacity) const {
    return FIELD_ADDR(this, kDataTableStartOffset + DataTableSizeFor(capacity));
  }

  void SetFirstEntry(int bucket, byte value) {
    DCHECK_LE(static_cast<unsigned>(bucket), NumberOfBuckets());
    setByte(GetBucketsStartOffset(), bucket, value);
  }

  int GetFirstEntry(int bucket) const {
    DCHECK_LE(static_cast<unsigned>(bucket), NumberOfBuckets());
    return getByte(GetBucketsStartOffset(), bucket);
  }

  // TODO(gsathya): Calculate all the various possible values for this
  // at compile time since capacity can only be 4 different values.
  Offset GetChainTableOffset() const {
    int nof_buckets = NumberOfBuckets();
    int capacity = nof_buckets * kLoadFactor;
    DCHECK_EQ(Capacity(), capacity);

    int data_table_size = DataTableSizeFor(capacity);
    int hash_table_size = nof_buckets;
    return kDataTableStartOffset + data_table_size + hash_table_size;
  }

  void SetNextEntry(int entry, int next_entry) {
    DCHECK_LT(static_cast<unsigned>(entry), Capacity());
    DCHECK_GE(static_cast<unsigned>(next_entry), 0);
    DCHECK(next_entry <= Capacity() || next_entry == kNotFound);
    setByte(GetChainTableOffset(), entry, next_entry);
  }

  int GetNextEntry(int entry) const {
    DCHECK_LT(entry, Capacity());
    return getByte(GetChainTableOffset(), entry);
  }

  Object* GetDataEntry(int entry, int relative_index) {
    DCHECK_LT(entry, Capacity());
    DCHECK_LE(static_cast<unsigned>(relative_index), Derived::kEntrySize);
    Offset entry_offset = GetDataEntryOffset(entry, relative_index);
    return READ_FIELD(this, entry_offset);
  }

  Object* KeyAt(int entry) const {
    DCHECK_LT(entry, Capacity());
    Offset entry_offset = GetDataEntryOffset(entry, Derived::kKeyIndex);
    return READ_FIELD(this, entry_offset);
  }

  int HashToBucket(int hash) const { return hash & (NumberOfBuckets() - 1); }

  int HashToFirstEntry(int hash) const {
    int bucket = HashToBucket(hash);
    int entry = GetFirstEntry(bucket);
    DCHECK(entry < Capacity() || entry == kNotFound);
    return entry;
  }

  void SetNumberOfBuckets(int num) { setByte(kNumberOfBucketsOffset, 0, num); }

  void SetNumberOfElements(int num) {
    DCHECK_LE(static_cast<unsigned>(num), Capacity());
    setByte(kNumberOfElementsOffset, 0, num);
  }

  void SetNumberOfDeletedElements(int num) {
    DCHECK_LE(static_cast<unsigned>(num), Capacity());
    setByte(kNumberOfDeletedElementsOffset, 0, num);
  }

  int FindEntry(Isolate* isolate, Object* key) {
    DisallowHeapAllocation no_gc;
    Object* hash = key->GetHash();

    if (hash->IsUndefined(isolate)) return kNotFound;
    int entry = HashToFirstEntry(Smi::ToInt(hash));

    // Walk the chain in the bucket to find the key.
    while (entry != kNotFound) {
      Object* candidate_key = KeyAt(entry);
      if (candidate_key->SameValueZero(key)) return entry;
      entry = GetNextEntry(entry);
    }
    return kNotFound;
  }

  static const Offset kNumberOfElementsOffset = kHeaderSize;
  static const Offset kNumberOfDeletedElementsOffset =
      kNumberOfElementsOffset + kOneByteSize;
  static const Offset kNumberOfBucketsOffset =
      kNumberOfDeletedElementsOffset + kOneByteSize;
  static const constexpr Offset kDataTableStartOffset =
      RoundUp<kPointerSize>(kNumberOfBucketsOffset);

  static constexpr int DataTableSizeFor(int capacity) {
    return capacity * Derived::kEntrySize * kPointerSize;
  }

  // This is used for accessing the non |DataTable| part of the
  // structure.
  byte getByte(Offset offset, ByteIndex index) const {
    DCHECK(offset < kDataTableStartOffset || offset >= GetBucketsStartOffset());
    return READ_BYTE_FIELD(this, offset + (index * kOneByteSize));
  }

  void setByte(Offset offset, ByteIndex index, byte value) {
    DCHECK(offset < kDataTableStartOffset || offset >= GetBucketsStartOffset());
    WRITE_BYTE_FIELD(this, offset + (index * kOneByteSize), value);
  }

  Offset GetDataEntryOffset(int entry, int relative_index) const {
    DCHECK_LT(entry, Capacity());
    int offset_in_datatable = entry * Derived::kEntrySize * kPointerSize;
    int offset_in_entry = relative_index * kPointerSize;
    return kDataTableStartOffset + offset_in_datatable + offset_in_entry;
  }

  int UsedCapacity() const {
    int used = NumberOfElements() + NumberOfDeletedElements();
    DCHECK_LE(used, Capacity());

    return used;
  }

 private:
  friend class OrderedHashMapHandler;
  friend class OrderedHashSetHandler;
  friend class CodeStubAssembler;
};

class SmallOrderedHashSet : public SmallOrderedHashTable<SmallOrderedHashSet> {
 public:
  DECL_CAST(SmallOrderedHashSet)

  DECL_PRINTER(SmallOrderedHashSet)

  static const int kKeyIndex = 0;
  static const int kEntrySize = 1;

  // Adds |value| to |table|, if the capacity isn't enough, a new
  // table is created. The original |table| is returned if there is
  // capacity to store |value| otherwise the new table is returned.
  static MaybeHandle<SmallOrderedHashSet> Add(Isolate* isolate,
                                              Handle<SmallOrderedHashSet> table,
                                              Handle<Object> key);
  static inline bool Is(Handle<HeapObject> table);
  static inline RootIndex GetMapRootIndex();
};

class SmallOrderedHashMap : public SmallOrderedHashTable<SmallOrderedHashMap> {
 public:
  DECL_CAST(SmallOrderedHashMap)

  DECL_PRINTER(SmallOrderedHashMap)

  static const int kKeyIndex = 0;
  static const int kValueIndex = 1;
  static const int kEntrySize = 2;

  // Adds |value| to |table|, if the capacity isn't enough, a new
  // table is created. The original |table| is returned if there is
  // capacity to store |value| otherwise the new table is returned.
  static MaybeHandle<SmallOrderedHashMap> Add(Isolate* isolate,
                                              Handle<SmallOrderedHashMap> table,
                                              Handle<Object> key,
                                              Handle<Object> value);
  static inline bool Is(Handle<HeapObject> table);
  static inline RootIndex GetMapRootIndex();
};

// TODO(gsathya): Rename this to OrderedHashTable, after we rename
// OrderedHashTable to LargeOrderedHashTable. Also set up a
// OrderedHashSetBase class as a base class for the two tables and use
// that instead of a HeapObject here.
template <class SmallTable, class LargeTable>
class OrderedHashTableHandler {
 public:
  typedef int Entry;

  static Handle<HeapObject> Allocate(Isolate* isolate, int capacity);
  static bool Delete(Handle<HeapObject> table, Handle<Object> key);
  static bool HasKey(Isolate* isolate, Handle<HeapObject> table,
                     Handle<Object> key);

  // TODO(gsathya): Move this to OrderedHashTable
  static const int OrderedHashTableMinSize =
      SmallOrderedHashTable<SmallTable>::kGrowthHack << 1;
};

class OrderedHashMapHandler
    : public OrderedHashTableHandler<SmallOrderedHashMap, OrderedHashMap> {
 public:
  static Handle<HeapObject> Add(Isolate* isolate, Handle<HeapObject> table,
                                Handle<Object> key, Handle<Object> value);
  static Handle<OrderedHashMap> AdjustRepresentation(
      Isolate* isolate, Handle<SmallOrderedHashMap> table);
};

class OrderedHashSetHandler
    : public OrderedHashTableHandler<SmallOrderedHashSet, OrderedHashSet> {
 public:
  static Handle<HeapObject> Add(Isolate* isolate, Handle<HeapObject> table,
                                Handle<Object> key);
  static Handle<OrderedHashSet> AdjustRepresentation(
      Isolate* isolate, Handle<SmallOrderedHashSet> table);
};

class JSCollectionIterator : public JSObject {
 public:
  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // [index]: The index into the data table.
  DECL_ACCESSORS(index, Object)

  // Dispatched behavior.
  DECL_PRINTER(JSCollectionIterator)

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kIndexOffset = kTableOffset + kPointerSize;
  static const int kSize = kIndexOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSCollectionIterator);
};

// OrderedHashTableIterator is an iterator that iterates over the keys and
// values of an OrderedHashTable.
//
// The iterator has a reference to the underlying OrderedHashTable data,
// [table], as well as the current [index] the iterator is at.
//
// When the OrderedHashTable is rehashed it adds a reference from the old table
// to the new table as well as storing enough data about the changes so that the
// iterator [index] can be adjusted accordingly.
//
// When the [Next] result from the iterator is requested, the iterator checks if
// there is a newer table that it needs to transition to.
template <class Derived, class TableType>
class OrderedHashTableIterator : public JSCollectionIterator {
 public:
  // Whether the iterator has more elements. This needs to be called before
  // calling |CurrentKey| and/or |CurrentValue|.
  bool HasMore();

  // Move the index forward one.
  void MoveNext() { set_index(Smi::FromInt(Smi::ToInt(index()) + 1)); }

  // Returns the current key of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Object* CurrentKey();

 private:
  // Transitions the iterator to the non obsolete backing store. This is a NOP
  // if the [table] is not obsolete.
  void Transition();

  DISALLOW_IMPLICIT_CONSTRUCTORS(OrderedHashTableIterator);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ORDERED_HASH_TABLE_H_
