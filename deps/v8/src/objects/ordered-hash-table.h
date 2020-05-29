// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ORDERED_HASH_TABLE_H_
#define V8_OBJECTS_ORDERED_HASH_TABLE_H_

#include "src/base/export-template.h"
#include "src/common/globals.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// OrderedHashTable is a HashTable with Object keys that preserves
// insertion order. There are Map and Set interfaces (OrderedHashMap
// and OrderedHashTable, below). It is meant to be used by JSMap/JSSet.
//
// Only Object keys are supported, with Object::SameValueZero() used as the
// equality operator and Object::GetHash() for the hash function.
//
// Based on the "Deterministic Hash Table" as described by Jason Orendorff at
// https://wiki.mozilla.org/User:Jorend/Deterministic_hash_tables
// Originally attributed to Tyler Close.
//
// Memory layout:
//   [0] : Prefix
//   [kPrefixSize]: element count
//   [kPrefixSize + 1]: deleted element count
//   [kPrefixSize + 2]: bucket count
//   [kPrefixSize + 3..(3 + NumberOfBuckets() - 1)]: "hash table",
//                            where each item is an offset into the
//                            data table (see below) where the first
//                            item in this bucket is stored.
//   [kPrefixSize + 3 + NumberOfBuckets()..length]: "data table", an
//                            array of length Capacity() * kEntrySize,
//                            where the first entrysize items are
//                            handled by the derived class and the
//                            item at kChainOffset is another entry
//                            into the data table indicating the next
//                            entry in this hash bucket.
//
// When we transition the table to a new version we obsolete it and reuse parts
// of the memory to store information how to transition an iterator to the new
// table:
//
// Memory layout for obsolete table:
//   [0] : Prefix
//   [kPrefixSize + 0]: bucket count
//   [kPrefixSize + 1]: Next newer table
//   [kPrefixSize + 2]: Number of removed holes or -1 when the table was
//                      cleared.
//   [kPrefixSize + 3..(3 + NumberOfRemovedHoles() - 1)]: The indexes
//                      of the removed holes.
//   [kPrefixSize + 3 + NumberOfRemovedHoles()..length]: Not used
template <class Derived, int entrysize>
class OrderedHashTable : public FixedArray {
 public:
  // Returns an OrderedHashTable (possibly |table|) with enough space
  // to add at least one new element.
  static MaybeHandle<Derived> EnsureGrowable(Isolate* isolate,
                                             Handle<Derived> table);

  // Returns an OrderedHashTable (possibly |table|) that's shrunken
  // if possible.
  static Handle<Derived> Shrink(Isolate* isolate, Handle<Derived> table);

  // Returns a new empty OrderedHashTable and records the clearing so that
  // existing iterators can be updated.
  static Handle<Derived> Clear(Isolate* isolate, Handle<Derived> table);

  // Returns true if the OrderedHashTable contains the key
  static bool HasKey(Isolate* isolate, Derived table, Object key);

  // Returns a true value if the OrderedHashTable contains the key and
  // the key has been deleted. This does not shrink the table.
  static bool Delete(Isolate* isolate, Derived table, Object key);

  int FindEntry(Isolate* isolate, Object key);

  int NumberOfElements() const {
    return Smi::ToInt(get(NumberOfElementsIndex()));
  }

  int NumberOfDeletedElements() const {
    return Smi::ToInt(get(NumberOfDeletedElementsIndex()));
  }

  // Returns the number of contiguous entries in the data table, starting at 0,
  // that either are real entries or have been deleted.
  int UsedCapacity() const {
    return NumberOfElements() + NumberOfDeletedElements();
  }

  int NumberOfBuckets() const {
    return Smi::ToInt(get(NumberOfBucketsIndex()));
  }

  // Returns an index into |this| for the given entry.
  int EntryToIndex(int entry) {
    return HashTableStartIndex() + NumberOfBuckets() + (entry * kEntrySize);
  }

  int HashToBucket(int hash) { return hash & (NumberOfBuckets() - 1); }

  int HashToEntry(int hash) {
    int bucket = HashToBucket(hash);
    Object entry = this->get(HashTableStartIndex() + bucket);
    return Smi::ToInt(entry);
  }

  int NextChainEntry(int entry) {
    Object next_entry = get(EntryToIndex(entry) + kChainOffset);
    return Smi::ToInt(next_entry);
  }

  // use KeyAt(i)->IsTheHole(isolate) to determine if this is a deleted entry.
  Object KeyAt(int entry) {
    DCHECK_LT(entry, this->UsedCapacity());
    return get(EntryToIndex(entry));
  }

  bool IsObsolete() { return !get(NextTableIndex()).IsSmi(); }

  // The next newer table. This is only valid if the table is obsolete.
  Derived NextTable() { return Derived::cast(get(NextTableIndex())); }

  // When the table is obsolete we store the indexes of the removed holes.
  int RemovedIndexAt(int index) {
    return Smi::ToInt(get(RemovedHolesIndex() + index));
  }

  // The extra +1 is for linking the bucket chains together.
  static const int kEntrySize = entrysize + 1;
  static const int kEntrySizeWithoutChain = entrysize;
  static const int kChainOffset = entrysize;

  static const int kNotFound = -1;
  static const int kMinCapacity = 4;

  static constexpr int PrefixIndex() { return 0; }

  static constexpr int NumberOfElementsIndex() { return Derived::kPrefixSize; }

  // The next table is stored at the same index as the nof elements.
  static constexpr int NextTableIndex() { return NumberOfElementsIndex(); }

  static constexpr int NumberOfDeletedElementsIndex() {
    return NumberOfElementsIndex() + 1;
  }

  static constexpr int NumberOfBucketsIndex() {
    return NumberOfDeletedElementsIndex() + 1;
  }

  static constexpr int HashTableStartIndex() {
    return NumberOfBucketsIndex() + 1;
  }

  static constexpr int RemovedHolesIndex() { return HashTableStartIndex(); }

  static constexpr int NumberOfElementsOffset() {
    return FixedArray::OffsetOfElementAt(NumberOfElementsIndex());
  }

  static constexpr int NextTableOffset() {
    return FixedArray::OffsetOfElementAt(NextTableIndex());
  }

  static constexpr int NumberOfDeletedElementsOffset() {
    return FixedArray::OffsetOfElementAt(NumberOfDeletedElementsIndex());
  }

  static constexpr int NumberOfBucketsOffset() {
    return FixedArray::OffsetOfElementAt(NumberOfBucketsIndex());
  }

  static constexpr int HashTableStartOffset() {
    return FixedArray::OffsetOfElementAt(HashTableStartIndex());
  }

  static const int kLoadFactor = 2;

  // NumberOfDeletedElements is set to kClearedTableSentinel when
  // the table is cleared, which allows iterator transitions to
  // optimize that case.
  static const int kClearedTableSentinel = -1;
  static constexpr int MaxCapacity() {
    return (FixedArray::kMaxLength - HashTableStartIndex()) /
           (1 + (kEntrySize * kLoadFactor));
  }

 protected:
  // Returns an OrderedHashTable with a capacity of at least |capacity|.
  static MaybeHandle<Derived> Allocate(
      Isolate* isolate, int capacity,
      AllocationType allocation = AllocationType::kYoung);

  static MaybeHandle<Derived> Rehash(Isolate* isolate, Handle<Derived> table);
  static MaybeHandle<Derived> Rehash(Isolate* isolate, Handle<Derived> table,
                                     int new_capacity);

  void SetNumberOfBuckets(int num) {
    set(NumberOfBucketsIndex(), Smi::FromInt(num));
  }

  void SetNumberOfElements(int num) {
    set(NumberOfElementsIndex(), Smi::FromInt(num));
  }

  void SetNumberOfDeletedElements(int num) {
    set(NumberOfDeletedElementsIndex(), Smi::FromInt(num));
  }

  // Returns the number elements that can fit into the allocated buffer.
  int Capacity() { return NumberOfBuckets() * kLoadFactor; }

  void SetNextTable(Derived next_table) { set(NextTableIndex(), next_table); }

  void SetRemovedIndexAt(int index, int removed_index) {
    return set(RemovedHolesIndex() + index, Smi::FromInt(removed_index));
  }

  OBJECT_CONSTRUCTORS(OrderedHashTable, FixedArray);

 private:
  friend class OrderedNameDictionaryHandler;
};

class V8_EXPORT_PRIVATE OrderedHashSet
    : public OrderedHashTable<OrderedHashSet, 1> {
 public:
  DECL_CAST(OrderedHashSet)

  static MaybeHandle<OrderedHashSet> Add(Isolate* isolate,
                                         Handle<OrderedHashSet> table,
                                         Handle<Object> value);
  static Handle<FixedArray> ConvertToKeysArray(Isolate* isolate,
                                               Handle<OrderedHashSet> table,
                                               GetKeysConversion convert);
  static MaybeHandle<OrderedHashSet> Rehash(Isolate* isolate,
                                            Handle<OrderedHashSet> table,
                                            int new_capacity);
  static MaybeHandle<OrderedHashSet> Rehash(Isolate* isolate,
                                            Handle<OrderedHashSet> table);
  static MaybeHandle<OrderedHashSet> Allocate(
      Isolate* isolate, int capacity,
      AllocationType allocation = AllocationType::kYoung);
  static HeapObject GetEmpty(ReadOnlyRoots ro_roots);
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
  static inline bool Is(Handle<HeapObject> table);
  static const int kPrefixSize = 0;

  OBJECT_CONSTRUCTORS(OrderedHashSet, OrderedHashTable<OrderedHashSet, 1>);
};

class V8_EXPORT_PRIVATE OrderedHashMap
    : public OrderedHashTable<OrderedHashMap, 2> {
 public:
  DECL_CAST(OrderedHashMap)

  // Returns a value if the OrderedHashMap contains the key, otherwise
  // returns undefined.
  static MaybeHandle<OrderedHashMap> Add(Isolate* isolate,
                                         Handle<OrderedHashMap> table,
                                         Handle<Object> key,
                                         Handle<Object> value);

  static MaybeHandle<OrderedHashMap> Allocate(
      Isolate* isolate, int capacity,
      AllocationType allocation = AllocationType::kYoung);
  static MaybeHandle<OrderedHashMap> Rehash(Isolate* isolate,
                                            Handle<OrderedHashMap> table,
                                            int new_capacity);
  static MaybeHandle<OrderedHashMap> Rehash(Isolate* isolate,
                                            Handle<OrderedHashMap> table);
  Object ValueAt(int entry);

  // This takes and returns raw Address values containing tagged Object
  // pointers because it is called via ExternalReference.
  static Address GetHash(Isolate* isolate, Address raw_key);

  static HeapObject GetEmpty(ReadOnlyRoots ro_roots);
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
  static inline bool Is(Handle<HeapObject> table);

  static const int kValueOffset = 1;
  static const int kPrefixSize = 0;

  OBJECT_CONSTRUCTORS(OrderedHashMap, OrderedHashTable<OrderedHashMap, 2>);
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
// The prefix size is calculated as the kPrefixSize * kTaggedSize.
//
// Memory layout: [ Prefix ] [ Header ]  [ Padding ] [ DataTable ] [ HashTable ]
// [ Chains ]
//
// The index are represented as bytes, on a 64 bit machine with
// kEntrySize = 1, capacity = 4 and entries = 2:
//
// [ 0 ] : Prefix
//
// Note: For the sake of brevity, the following start with index 0
// but, they actually start from kPrefixSize * kTaggedSize to
// account for the the prefix.
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
  using Offset = int;

  // ByteIndex points to a index in the table that needs to be
  // converted to an Offset.
  using ByteIndex = int;

  void Initialize(Isolate* isolate, int capacity);

  static Handle<Derived> Allocate(
      Isolate* isolate, int capacity,
      AllocationType allocation = AllocationType::kYoung);

  // Returns a true if the OrderedHashTable contains the key
  bool HasKey(Isolate* isolate, Handle<Object> key);

  // Returns a true value if the table contains the key and
  // the key has been deleted. This does not shrink the table.
  static bool Delete(Isolate* isolate, Derived table, Object key);

  // Returns an SmallOrderedHashTable (possibly |table|) with enough
  // space to add at least one new element. Returns empty handle if
  // we've already reached MaxCapacity.
  static MaybeHandle<Derived> Grow(Isolate* isolate, Handle<Derived> table);

  int FindEntry(Isolate* isolate, Object key);
  static Handle<Derived> Shrink(Isolate* isolate, Handle<Derived> table);

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
    int total_size = DataTableStartOffset() + data_table_size +
                     hash_table_size + chain_table_size;

    return RoundUp(total_size, kTaggedSize);
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
    int nof_elements = getByte(NumberOfElementsOffset(), 0);
    DCHECK_LE(nof_elements, Capacity());

    return nof_elements;
  }

  int NumberOfDeletedElements() const {
    int nof_deleted_elements = getByte(NumberOfDeletedElementsOffset(), 0);
    DCHECK_LE(nof_deleted_elements, Capacity());

    return nof_deleted_elements;
  }

  int NumberOfBuckets() const { return getByte(NumberOfBucketsOffset(), 0); }

  V8_INLINE Object KeyAt(int entry) const;

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
  static Handle<Derived> Rehash(Isolate* isolate, Handle<Derived> table,
                                int new_capacity);

  void SetDataEntry(int entry, int relative_index, Object value);

  // TODO(gsathya): Calculate all the various possible values for this
  // at compile time since capacity can only be 4 different values.
  Offset GetBucketsStartOffset() const {
    int capacity = Capacity();
    int data_table_size = DataTableSizeFor(capacity);
    return DataTableStartOffset() + data_table_size;
  }

  Address GetHashTableStartAddress(int capacity) const {
    return FIELD_ADDR(*this,
                      DataTableStartOffset() + DataTableSizeFor(capacity));
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
    return DataTableStartOffset() + data_table_size + hash_table_size;
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

  V8_INLINE Object GetDataEntry(int entry, int relative_index);

  int HashToBucket(int hash) const { return hash & (NumberOfBuckets() - 1); }

  int HashToFirstEntry(int hash) const {
    int bucket = HashToBucket(hash);
    int entry = GetFirstEntry(bucket);
    DCHECK(entry < Capacity() || entry == kNotFound);
    return entry;
  }

  void SetNumberOfBuckets(int num) { setByte(NumberOfBucketsOffset(), 0, num); }

  void SetNumberOfElements(int num) {
    DCHECK_LE(static_cast<unsigned>(num), Capacity());
    setByte(NumberOfElementsOffset(), 0, num);
  }

  void SetNumberOfDeletedElements(int num) {
    DCHECK_LE(static_cast<unsigned>(num), Capacity());
    setByte(NumberOfDeletedElementsOffset(), 0, num);
  }

  static constexpr Offset PrefixOffset() { return kHeaderSize; }

  static constexpr Offset NumberOfElementsOffset() {
    return PrefixOffset() + (Derived::kPrefixSize * kTaggedSize);
  }

  static constexpr Offset NumberOfDeletedElementsOffset() {
    return NumberOfElementsOffset() + kOneByteSize;
  }

  static constexpr Offset NumberOfBucketsOffset() {
    return NumberOfDeletedElementsOffset() + kOneByteSize;
  }

  static constexpr Offset PaddingOffset() {
    return NumberOfBucketsOffset() + kOneByteSize;
  }

  static constexpr size_t PaddingSize() {
    return RoundUp<kTaggedSize>(PaddingOffset()) - PaddingOffset();
  }

  static constexpr Offset DataTableStartOffset() {
    return PaddingOffset() + PaddingSize();
  }

  static constexpr int DataTableSizeFor(int capacity) {
    return capacity * Derived::kEntrySize * kTaggedSize;
  }

  // This is used for accessing the non |DataTable| part of the
  // structure.
  byte getByte(Offset offset, ByteIndex index) const {
    DCHECK(offset < DataTableStartOffset() ||
           offset >= GetBucketsStartOffset());
    return ReadField<byte>(offset + (index * kOneByteSize));
  }

  void setByte(Offset offset, ByteIndex index, byte value) {
    DCHECK(offset < DataTableStartOffset() ||
           offset >= GetBucketsStartOffset());
    WriteField<byte>(offset + (index * kOneByteSize), value);
  }

  Offset GetDataEntryOffset(int entry, int relative_index) const {
    DCHECK_LT(entry, Capacity());
    int offset_in_datatable = entry * Derived::kEntrySize * kTaggedSize;
    int offset_in_entry = relative_index * kTaggedSize;
    return DataTableStartOffset() + offset_in_datatable + offset_in_entry;
  }

  int UsedCapacity() const {
    int used = NumberOfElements() + NumberOfDeletedElements();
    DCHECK_LE(used, Capacity());

    return used;
  }

 private:
  friend class OrderedHashMapHandler;
  friend class OrderedHashSetHandler;
  friend class OrderedNameDictionaryHandler;
  friend class CodeStubAssembler;

  OBJECT_CONSTRUCTORS(SmallOrderedHashTable, HeapObject);
};

class SmallOrderedHashSet : public SmallOrderedHashTable<SmallOrderedHashSet> {
 public:
  DECL_CAST(SmallOrderedHashSet)

  DECL_PRINTER(SmallOrderedHashSet)
  EXPORT_DECL_VERIFIER(SmallOrderedHashSet)

  static const int kKeyIndex = 0;
  static const int kEntrySize = 1;
  static const int kPrefixSize = 0;

  // Adds |value| to |table|, if the capacity isn't enough, a new
  // table is created. The original |table| is returned if there is
  // capacity to store |value| otherwise the new table is returned.
  V8_EXPORT_PRIVATE static MaybeHandle<SmallOrderedHashSet> Add(
      Isolate* isolate, Handle<SmallOrderedHashSet> table, Handle<Object> key);
  V8_EXPORT_PRIVATE static bool Delete(Isolate* isolate,
                                       SmallOrderedHashSet table, Object key);
  V8_EXPORT_PRIVATE bool HasKey(Isolate* isolate, Handle<Object> key);

  static inline bool Is(Handle<HeapObject> table);
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
  static Handle<SmallOrderedHashSet> Rehash(Isolate* isolate,
                                            Handle<SmallOrderedHashSet> table,
                                            int new_capacity);
  OBJECT_CONSTRUCTORS(SmallOrderedHashSet,
                      SmallOrderedHashTable<SmallOrderedHashSet>);
};

STATIC_ASSERT(kSmallOrderedHashSetMinCapacity ==
              SmallOrderedHashSet::kMinCapacity);

class SmallOrderedHashMap : public SmallOrderedHashTable<SmallOrderedHashMap> {
 public:
  DECL_CAST(SmallOrderedHashMap)

  DECL_PRINTER(SmallOrderedHashMap)
  EXPORT_DECL_VERIFIER(SmallOrderedHashMap)

  static const int kKeyIndex = 0;
  static const int kValueIndex = 1;
  static const int kEntrySize = 2;
  static const int kPrefixSize = 0;

  // Adds |value| to |table|, if the capacity isn't enough, a new
  // table is created. The original |table| is returned if there is
  // capacity to store |value| otherwise the new table is returned.
  V8_EXPORT_PRIVATE static MaybeHandle<SmallOrderedHashMap> Add(
      Isolate* isolate, Handle<SmallOrderedHashMap> table, Handle<Object> key,
      Handle<Object> value);
  V8_EXPORT_PRIVATE static bool Delete(Isolate* isolate,
                                       SmallOrderedHashMap table, Object key);
  V8_EXPORT_PRIVATE bool HasKey(Isolate* isolate, Handle<Object> key);
  static inline bool Is(Handle<HeapObject> table);
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);

  static Handle<SmallOrderedHashMap> Rehash(Isolate* isolate,
                                            Handle<SmallOrderedHashMap> table,
                                            int new_capacity);

  OBJECT_CONSTRUCTORS(SmallOrderedHashMap,
                      SmallOrderedHashTable<SmallOrderedHashMap>);
};

STATIC_ASSERT(kSmallOrderedHashMapMinCapacity ==
              SmallOrderedHashMap::kMinCapacity);

// TODO(gsathya): Rename this to OrderedHashTable, after we rename
// OrderedHashTable to LargeOrderedHashTable. Also set up a
// OrderedHashSetBase class as a base class for the two tables and use
// that instead of a HeapObject here.
template <class SmallTable, class LargeTable>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) OrderedHashTableHandler {
 public:
  using Entry = int;

  static MaybeHandle<HeapObject> Allocate(Isolate* isolate, int capacity);
  static bool Delete(Isolate* isolate, Handle<HeapObject> table,
                     Handle<Object> key);
  static bool HasKey(Isolate* isolate, Handle<HeapObject> table,
                     Handle<Object> key);

  // TODO(gsathya): Move this to OrderedHashTable
  static const int OrderedHashTableMinSize =
      SmallOrderedHashTable<SmallTable>::kGrowthHack << 1;
};

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    OrderedHashTableHandler<SmallOrderedHashMap, OrderedHashMap>;

class V8_EXPORT_PRIVATE OrderedHashMapHandler
    : public OrderedHashTableHandler<SmallOrderedHashMap, OrderedHashMap> {
 public:
  static MaybeHandle<HeapObject> Add(Isolate* isolate, Handle<HeapObject> table,
                                     Handle<Object> key, Handle<Object> value);
  static MaybeHandle<OrderedHashMap> AdjustRepresentation(
      Isolate* isolate, Handle<SmallOrderedHashMap> table);
};

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    OrderedHashTableHandler<SmallOrderedHashSet, OrderedHashSet>;

class V8_EXPORT_PRIVATE OrderedHashSetHandler
    : public OrderedHashTableHandler<SmallOrderedHashSet, OrderedHashSet> {
 public:
  static MaybeHandle<HeapObject> Add(Isolate* isolate, Handle<HeapObject> table,
                                     Handle<Object> key);
  static MaybeHandle<OrderedHashSet> AdjustRepresentation(
      Isolate* isolate, Handle<SmallOrderedHashSet> table);
};

class OrderedNameDictionary
    : public OrderedHashTable<OrderedNameDictionary, 3> {
 public:
  DECL_CAST(OrderedNameDictionary)

  V8_EXPORT_PRIVATE static MaybeHandle<OrderedNameDictionary> Add(
      Isolate* isolate, Handle<OrderedNameDictionary> table, Handle<Name> key,
      Handle<Object> value, PropertyDetails details);

  V8_EXPORT_PRIVATE void SetEntry(int entry, Object key, Object value,
                                  PropertyDetails details);

  V8_EXPORT_PRIVATE static Handle<OrderedNameDictionary> DeleteEntry(
      Isolate* isolate, Handle<OrderedNameDictionary> table, int entry);

  static MaybeHandle<OrderedNameDictionary> Allocate(
      Isolate* isolate, int capacity,
      AllocationType allocation = AllocationType::kYoung);

  static MaybeHandle<OrderedNameDictionary> Rehash(
      Isolate* isolate, Handle<OrderedNameDictionary> table, int new_capacity);

  // Returns the value for entry.
  inline Object ValueAt(int entry);

  // Set the value for entry.
  inline void ValueAtPut(int entry, Object value);

  // Returns the property details for the property at entry.
  inline PropertyDetails DetailsAt(int entry);

  // Set the details for entry.
  inline void DetailsAtPut(int entry, PropertyDetails value);

  inline void SetHash(int hash);
  inline int Hash();

  static HeapObject GetEmpty(ReadOnlyRoots ro_roots);
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
  static inline bool Is(Handle<HeapObject> table);

  static const int kValueOffset = 1;
  static const int kPropertyDetailsOffset = 2;
  static const int kPrefixSize = 1;

  OBJECT_CONSTRUCTORS(OrderedNameDictionary,
                      OrderedHashTable<OrderedNameDictionary, 3>);
};

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    OrderedHashTableHandler<SmallOrderedNameDictionary, OrderedNameDictionary>;

class V8_EXPORT_PRIVATE OrderedNameDictionaryHandler
    : public OrderedHashTableHandler<SmallOrderedNameDictionary,
                                     OrderedNameDictionary> {
 public:
  static MaybeHandle<HeapObject> Add(Isolate* isolate, Handle<HeapObject> table,
                                     Handle<Name> key, Handle<Object> value,
                                     PropertyDetails details);
  static Handle<HeapObject> Shrink(Isolate* isolate, Handle<HeapObject> table);

  static Handle<HeapObject> DeleteEntry(Isolate* isolate,
                                        Handle<HeapObject> table, int entry);
  static int FindEntry(Isolate* isolate, HeapObject table, Name key);
  static void SetEntry(HeapObject table, int entry, Object key, Object value,
                       PropertyDetails details);

  // Returns the value for entry.
  static Object ValueAt(HeapObject table, int entry);

  // Set the value for entry.
  static void ValueAtPut(HeapObject table, int entry, Object value);

  // Returns the property details for the property at entry.
  static PropertyDetails DetailsAt(HeapObject table, int entry);

  // Set the details for entry.
  static void DetailsAtPut(HeapObject table, int entry, PropertyDetails value);

  static Name KeyAt(HeapObject table, int entry);

  static void SetHash(HeapObject table, int hash);
  static int Hash(HeapObject table);

  static int NumberOfElements(HeapObject table);
  static int Capacity(HeapObject table);

  static const int kNotFound = -1;

 protected:
  static MaybeHandle<OrderedNameDictionary> AdjustRepresentation(
      Isolate* isolate, Handle<SmallOrderedNameDictionary> table);
};

class SmallOrderedNameDictionary
    : public SmallOrderedHashTable<SmallOrderedNameDictionary> {
 public:
  DECL_CAST(SmallOrderedNameDictionary)

  DECL_PRINTER(SmallOrderedNameDictionary)
  DECL_VERIFIER(SmallOrderedNameDictionary)

  // Returns the value for entry.
  inline Object ValueAt(int entry);

  static Handle<SmallOrderedNameDictionary> Rehash(
      Isolate* isolate, Handle<SmallOrderedNameDictionary> table,
      int new_capacity);

  V8_EXPORT_PRIVATE static Handle<SmallOrderedNameDictionary> DeleteEntry(
      Isolate* isolate, Handle<SmallOrderedNameDictionary> table, int entry);

  // Set the value for entry.
  inline void ValueAtPut(int entry, Object value);

  // Returns the property details for the property at entry.
  inline PropertyDetails DetailsAt(int entry);

  // Set the details for entry.
  inline void DetailsAtPut(int entry, PropertyDetails value);

  inline void SetHash(int hash);
  inline int Hash();

  static const int kKeyIndex = 0;
  static const int kValueIndex = 1;
  static const int kPropertyDetailsIndex = 2;
  static const int kEntrySize = 3;
  static const int kPrefixSize = 1;

  // Adds |value| to |table|, if the capacity isn't enough, a new
  // table is created. The original |table| is returned if there is
  // capacity to store |value| otherwise the new table is returned.
  V8_EXPORT_PRIVATE static MaybeHandle<SmallOrderedNameDictionary> Add(
      Isolate* isolate, Handle<SmallOrderedNameDictionary> table,
      Handle<Name> key, Handle<Object> value, PropertyDetails details);

  V8_EXPORT_PRIVATE void SetEntry(int entry, Object key, Object value,
                                  PropertyDetails details);

  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
  static inline bool Is(Handle<HeapObject> table);

  OBJECT_CONSTRUCTORS(SmallOrderedNameDictionary,
                      SmallOrderedHashTable<SmallOrderedNameDictionary>);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ORDERED_HASH_TABLE_H_
