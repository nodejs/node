// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_TABLE_H_
#define V8_OBJECTS_HASH_TABLE_H_

#include "src/base/compiler-specific.h"
#include "src/globals.h"
#include "src/objects/fixed-array.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// HashTable is a subclass of FixedArray that implements a hash table
// that uses open addressing and quadratic probing.
//
// In order for the quadratic probing to work, elements that have not
// yet been used and elements that have been deleted are
// distinguished.  Probing continues when deleted elements are
// encountered and stops when unused elements are encountered.
//
// - Elements with key == undefined have not been used yet.
// - Elements with key == the_hole have been deleted.
//
// The hash table class is parameterized with a Shape.
// Shape must be a class with the following interface:
//   class ExampleShape {
//    public:
//     // Tells whether key matches other.
//     static bool IsMatch(Key key, Object* other);
//     // Returns the hash value for key.
//     static uint32_t Hash(Isolate* isolate, Key key);
//     // Returns the hash value for object.
//     static uint32_t HashForObject(Isolate* isolate, Object* object);
//     // Convert key to an object.
//     static inline Handle<Object> AsHandle(Isolate* isolate, Key key);
//     // The prefix size indicates number of elements in the beginning
//     // of the backing storage.
//     static const int kPrefixSize = ..;
//     // The Element size indicates number of elements per entry.
//     static const int kEntrySize = ..;
//     // Indicates whether IsMatch can deal with other being the_hole (a
//     // deleted entry).
//     static const bool kNeedsHoleCheck = ..;
//   };
// The prefix size indicates an amount of memory in the
// beginning of the backing storage that can be used for non-element
// information by subclasses.

template <typename KeyT>
class BaseShape {
 public:
  typedef KeyT Key;
  static inline int GetMapRootIndex();
  static const bool kNeedsHoleCheck = true;
  static Object* Unwrap(Object* key) { return key; }
  static bool IsKey(Isolate* isolate, Object* key) {
    return IsLive(isolate, key);
  }
  static inline bool IsLive(Isolate* isolate, Object* key);
};

class V8_EXPORT_PRIVATE HashTableBase : public NON_EXPORTED_BASE(FixedArray) {
 public:
  // Returns the number of elements in the hash table.
  inline int NumberOfElements() const;

  // Returns the number of deleted elements in the hash table.
  inline int NumberOfDeletedElements() const;

  // Returns the capacity of the hash table.
  inline int Capacity() const;

  // ElementAdded should be called whenever an element is added to a
  // hash table.
  inline void ElementAdded();

  // ElementRemoved should be called whenever an element is removed from
  // a hash table.
  inline void ElementRemoved();
  inline void ElementsRemoved(int n);

  // Computes the required capacity for a table holding the given
  // number of elements. May be more than HashTable::kMaxCapacity.
  static inline int ComputeCapacity(int at_least_space_for);

  // Compute the probe offset (quadratic probing).
  INLINE(static uint32_t GetProbeOffset(uint32_t n)) {
    return (n + n * n) >> 1;
  }

  static const int kNumberOfElementsIndex = 0;
  static const int kNumberOfDeletedElementsIndex = 1;
  static const int kCapacityIndex = 2;
  static const int kPrefixStartIndex = 3;

  // Constant used for denoting a absent entry.
  static const int kNotFound = -1;

  // Minimum capacity for newly created hash tables.
  static const int kMinCapacity = 4;

 protected:
  // Update the number of elements in the hash table.
  inline void SetNumberOfElements(int nof);

  // Update the number of deleted elements in the hash table.
  inline void SetNumberOfDeletedElements(int nod);

  // Returns probe entry.
  static uint32_t GetProbe(uint32_t hash, uint32_t number, uint32_t size) {
    DCHECK(base::bits::IsPowerOfTwo(size));
    return (hash + GetProbeOffset(number)) & (size - 1);
  }

  inline static uint32_t FirstProbe(uint32_t hash, uint32_t size) {
    return hash & (size - 1);
  }

  inline static uint32_t NextProbe(uint32_t last, uint32_t number,
                                   uint32_t size) {
    return (last + number) & (size - 1);
  }
};

template <typename Derived, typename Shape>
class HashTable : public HashTableBase {
 public:
  typedef Shape ShapeT;
  typedef typename Shape::Key Key;

  // Returns a new HashTable object.
  MUST_USE_RESULT static Handle<Derived> New(
      Isolate* isolate, int at_least_space_for,
      PretenureFlag pretenure = NOT_TENURED,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  DECL_CAST(HashTable)

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Find entry for key otherwise return kNotFound.
  inline int FindEntry(Key key);
  inline int FindEntry(Isolate* isolate, Key key, int32_t hash);
  int FindEntry(Isolate* isolate, Key key);

  // Rehashes the table in-place.
  void Rehash();

  // Tells whether k is a real key.  The hole and undefined are not allowed
  // as keys and can be used to indicate missing or deleted elements.
  static bool IsKey(Isolate* isolate, Object* k) {
    return Shape::IsKey(isolate, k);
  }

  inline bool ToKey(Isolate* isolate, int entry, Object** out_k) {
    Object* k = KeyAt(entry);
    if (!IsKey(isolate, k)) return false;
    *out_k = Shape::Unwrap(k);
    return true;
  }

  // Returns the key at entry.
  Object* KeyAt(int entry) { return get(EntryToIndex(entry) + kEntryKeyIndex); }

  static const int kElementsStartIndex = kPrefixStartIndex + Shape::kPrefixSize;
  static const int kEntrySize = Shape::kEntrySize;
  STATIC_ASSERT(kEntrySize > 0);
  static const int kEntryKeyIndex = 0;
  static const int kElementsStartOffset =
      kHeaderSize + kElementsStartIndex * kPointerSize;
  // Maximal capacity of HashTable. Based on maximal length of underlying
  // FixedArray. Staying below kMaxCapacity also ensures that EntryToIndex
  // cannot overflow.
  static const int kMaxCapacity =
      (FixedArray::kMaxLength - kElementsStartIndex) / kEntrySize;

  // Maximum length to create a regular HashTable (aka. non large object).
  static const int kMaxRegularCapacity = 16384;

  // Returns the index for an entry (of the key)
  static constexpr inline int EntryToIndex(int entry) {
    return (entry * kEntrySize) + kElementsStartIndex;
  }

  // Ensure enough space for n additional elements.
  MUST_USE_RESULT static Handle<Derived> EnsureCapacity(
      Handle<Derived> table, int n, PretenureFlag pretenure = NOT_TENURED);

  // Returns true if this table has sufficient capacity for adding n elements.
  bool HasSufficientCapacityToAdd(int number_of_additional_elements);

 protected:
  friend class ObjectHashTable;

  MUST_USE_RESULT static Handle<Derived> NewInternal(Isolate* isolate,
                                                     int capacity,
                                                     PretenureFlag pretenure);

  // Find the entry at which to insert element with the given key that
  // has the given hash value.
  uint32_t FindInsertionEntry(uint32_t hash);

  // Attempt to shrink hash table after removal of key.
  MUST_USE_RESULT static Handle<Derived> Shrink(Handle<Derived> table);

 private:
  // Ensure that kMaxRegularCapacity yields a non-large object dictionary.
  STATIC_ASSERT(EntryToIndex(kMaxRegularCapacity) < kMaxRegularLength);
  STATIC_ASSERT(v8::base::bits::IsPowerOfTwo(kMaxRegularCapacity));
  static const int kMaxRegularEntry = kMaxRegularCapacity / kEntrySize;
  static const int kMaxRegularIndex = EntryToIndex(kMaxRegularEntry);
  STATIC_ASSERT(OffsetOfElementAt(kMaxRegularIndex) <
                kMaxRegularHeapObjectSize);

  // Sets the capacity of the hash table.
  void SetCapacity(int capacity) {
    // To scale a computed hash code to fit within the hash table, we
    // use bit-wise AND with a mask, so the capacity must be positive
    // and non-zero.
    DCHECK_GT(capacity, 0);
    DCHECK_LE(capacity, kMaxCapacity);
    set(kCapacityIndex, Smi::FromInt(capacity));
  }

  // Returns _expected_ if one of entries given by the first _probe_ probes is
  // equal to  _expected_. Otherwise, returns the entry given by the probe
  // number _probe_.
  uint32_t EntryForProbe(Object* k, int probe, uint32_t expected);

  void Swap(uint32_t entry1, uint32_t entry2, WriteBarrierMode mode);

  // Rehashes this hash-table into the new table.
  void Rehash(Derived* new_table);
};

// HashTableKey is an abstract superclass for virtual key behavior.
class HashTableKey {
 public:
  explicit HashTableKey(uint32_t hash) : hash_(hash) {}

  // Returns whether the other object matches this key.
  virtual bool IsMatch(Object* other) = 0;
  // Returns the hash value for this key.
  // Required.
  virtual ~HashTableKey() {}

  uint32_t Hash() const { return hash_; }

 protected:
  void set_hash(uint32_t hash) {
    DCHECK_EQ(0, hash_);
    hash_ = hash;
  }

 private:
  uint32_t hash_ = 0;
};

class ObjectHashTableShape : public BaseShape<Handle<Object>> {
 public:
  static inline bool IsMatch(Handle<Object> key, Object* other);
  static inline uint32_t Hash(Isolate* isolate, Handle<Object> key);
  static inline uint32_t HashForObject(Isolate* isolate, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntryValueIndex = 1;
  static const int kEntrySize = 2;
  static const bool kNeedsHoleCheck = false;
};

// ObjectHashTable maps keys that are arbitrary objects to object values by
// using the identity hash of the key for hashing purposes.
class ObjectHashTable
    : public HashTable<ObjectHashTable, ObjectHashTableShape> {
  typedef HashTable<ObjectHashTable, ObjectHashTableShape> DerivedHashTable;

 public:
  DECL_CAST(ObjectHashTable)

  // Attempt to shrink hash table after removal of key.
  MUST_USE_RESULT static inline Handle<ObjectHashTable> Shrink(
      Handle<ObjectHashTable> table);

  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object* Lookup(Handle<Object> key);
  Object* Lookup(Handle<Object> key, int32_t hash);
  Object* Lookup(Isolate* isolate, Handle<Object> key, int32_t hash);

  // Returns the value at entry.
  Object* ValueAt(int entry);

  // Adds (or overwrites) the value associated with the given key.
  static Handle<ObjectHashTable> Put(Handle<ObjectHashTable> table,
                                     Handle<Object> key, Handle<Object> value);
  static Handle<ObjectHashTable> Put(Handle<ObjectHashTable> table,
                                     Handle<Object> key, Handle<Object> value,
                                     int32_t hash);

  // Returns an ObjectHashTable (possibly |table|) where |key| has been removed.
  static Handle<ObjectHashTable> Remove(Handle<ObjectHashTable> table,
                                        Handle<Object> key, bool* was_present);
  static Handle<ObjectHashTable> Remove(Handle<ObjectHashTable> table,
                                        Handle<Object> key, bool* was_present,
                                        int32_t hash);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + ObjectHashTableShape::kEntryValueIndex;
  }

 protected:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Object* key, Object* value);
  void RemoveEntry(int entry);
};

class ObjectHashSetShape : public ObjectHashTableShape {
 public:
  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class ObjectHashSet : public HashTable<ObjectHashSet, ObjectHashSetShape> {
 public:
  static Handle<ObjectHashSet> Add(Handle<ObjectHashSet> set,
                                   Handle<Object> key);

  inline bool Has(Isolate* isolate, Handle<Object> key, int32_t hash);
  inline bool Has(Isolate* isolate, Handle<Object> key);

  DECL_CAST(ObjectHashSet)
};

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
  static Handle<Derived> EnsureGrowable(Handle<Derived> table);

  // Returns an OrderedHashTable (possibly |table|) that's shrunken
  // if possible.
  static Handle<Derived> Shrink(Handle<Derived> table);

  // Returns a new empty OrderedHashTable and records the clearing so that
  // existing iterators can be updated.
  static Handle<Derived> Clear(Handle<Derived> table);

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
      uint32_t hash = ComputeIntegerHash(Smi::ToInt(key));
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
  static Handle<Derived> Rehash(Handle<Derived> table, int new_capacity);

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

  static Handle<OrderedHashSet> Add(Handle<OrderedHashSet> table,
                                    Handle<Object> value);
  static Handle<FixedArray> ConvertToKeysArray(Handle<OrderedHashSet> table,
                                               GetKeysConversion convert);
  static HeapObject* GetEmpty(Isolate* isolate);
  static inline int GetMapRootIndex();
};

class OrderedHashMap : public OrderedHashTable<OrderedHashMap, 2> {
 public:
  DECL_CAST(OrderedHashMap)

  // Returns a value if the OrderedHashMap contains the key, otherwise
  // returns undefined.
  static Handle<OrderedHashMap> Add(Handle<OrderedHashMap> table,
                                    Handle<Object> key, Handle<Object> value);
  Object* ValueAt(int entry);

  static Object* GetHash(Isolate* isolate, Object* key);

  static HeapObject* GetEmpty(Isolate* isolate);
  static inline int GetMapRootIndex();

  static const int kValueOffset = 1;
};

class WeakHashTableShape : public BaseShape<Handle<Object>> {
 public:
  static inline bool IsMatch(Handle<Object> key, Object* other);
  static inline uint32_t Hash(Isolate* isolate, Handle<Object> key);
  static inline uint32_t HashForObject(Isolate* isolate, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Object> key);
  static inline int GetMapRootIndex();
  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
  static const bool kNeedsHoleCheck = false;
};

// WeakHashTable maps keys that are arbitrary heap objects to heap object
// values. The table wraps the keys in weak cells and store values directly.
// Thus it references keys weakly and values strongly.
class WeakHashTable : public HashTable<WeakHashTable, WeakHashTableShape> {
  typedef HashTable<WeakHashTable, WeakHashTableShape> DerivedHashTable;

 public:
  DECL_CAST(WeakHashTable)

  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object* Lookup(Handle<HeapObject> key);

  // Adds (or overwrites) the value associated with the given key. Mapping a
  // key to the hole value causes removal of the whole entry.
  MUST_USE_RESULT static Handle<WeakHashTable> Put(Handle<WeakHashTable> table,
                                                   Handle<HeapObject> key,
                                                   Handle<HeapObject> value);

 private:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Handle<WeakCell> key, Handle<HeapObject> value);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + 1;
  }
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
// Memory layout: [ Header ] [ HashTable ] [ Chains ] [ Padding ] [ DataTable ]
//
// On a 64 bit machine with capacity = 4 and 2 entries,
//
// [ Header ]  :
//    [0  .. 7]  : Number of elements
//    [8  .. 15] : Number of deleted elements
//    [16 .. 23] : Number of buckets
//
// [ HashTable ] :
//    [24 .. 31] : First chain-link for bucket 1
//    [32 .. 40] : First chain-link for bucket 2
//
// [ Chains ] :
//    [40 .. 47] : Next chain link for entry 1
//    [48 .. 55] : Next chain link for entry 2
//    [56 .. 63] : Next chain link for entry 3
//    [64 .. 71] : Next chain link for entry 4
//
// [ Padding ] :
//    [72 .. 127] : Padding
//
// [ DataTable ] :
//    [128 .. 128 + kEntrySize - 1] : Entry 1
//    [128 + kEntrySize .. 128 + kEntrySize + kEntrySize - 1] : Entry 2
//
template <class Derived>
class SmallOrderedHashTable : public HeapObject {
 public:
  void Initialize(Isolate* isolate, int capacity);

  static Handle<Derived> Allocate(Isolate* isolate, int capacity,
                                  PretenureFlag pretenure = NOT_TENURED);

  // Returns a true if the OrderedHashTable contains the key
  bool HasKey(Isolate* isolate, Handle<Object> key);

  // Iterates only fields in the DataTable.
  class BodyDescriptor;

  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

  // Returns an SmallOrderedHashTable (possibly |table|) with enough
  // space to add at least one new element.
  static Handle<Derived> Grow(Handle<Derived> table);

  static Handle<Derived> Rehash(Handle<Derived> table, int new_capacity);

  void SetDataEntry(int entry, int relative_index, Object* value);

  static int GetDataTableStartOffset(int capacity) {
    int nof_buckets = capacity / kLoadFactor;
    int nof_chain_entries = capacity;

    int padding_index = kBucketsStartOffset + nof_buckets + nof_chain_entries;
    int padding_offset = padding_index * kBitsPerByte;

    return ((padding_offset + kPointerSize - 1) / kPointerSize) * kPointerSize;
  }

  int GetDataTableStartOffset() const {
    return GetDataTableStartOffset(Capacity());
  }

  static int Size(int capacity) {
    int data_table_start = GetDataTableStartOffset(capacity);
    int data_table_size = capacity * Derived::kEntrySize * kBitsPerPointer;
    return data_table_start + data_table_size;
  }

  int Size() const { return Size(Capacity()); }

  void SetFirstEntry(int bucket, byte value) {
    set(kBucketsStartOffset + bucket, value);
  }

  int GetFirstEntry(int bucket) const {
    return get(kBucketsStartOffset + bucket);
  }

  void SetNextEntry(int entry, int next_entry) {
    set(GetChainTableOffset() + entry, next_entry);
  }

  int GetNextEntry(int entry) const {
    return get(GetChainTableOffset() + entry);
  }

  Object* GetDataEntry(int entry, int relative_index) {
    int entry_offset = GetDataEntryOffset(entry, relative_index);
    return READ_FIELD(this, entry_offset);
  }

  Object* KeyAt(int entry) const {
    int entry_offset = GetDataEntryOffset(entry, Derived::kKeyIndex);
    return READ_FIELD(this, entry_offset);
  }

  int HashToBucket(int hash) const { return hash & (NumberOfBuckets() - 1); }

  int HashToFirstEntry(int hash) const {
    int bucket = HashToBucket(hash);
    int entry = GetFirstEntry(bucket);
    return entry;
  }

  int GetChainTableOffset() const {
    return kBucketsStartOffset + NumberOfBuckets();
  }

  void SetNumberOfBuckets(int num) { set(kNumberOfBucketsOffset, num); }

  void SetNumberOfElements(int num) { set(kNumberOfElementsOffset, num); }

  void SetNumberOfDeletedElements(int num) {
    set(kNumberOfDeletedElementsOffset, num);
  }

  int NumberOfElements() const { return get(kNumberOfElementsOffset); }

  int NumberOfDeletedElements() const {
    return get(kNumberOfDeletedElementsOffset);
  }

  int NumberOfBuckets() const { return get(kNumberOfBucketsOffset); }

  static const byte kNotFound = 0xFF;
  static const int kMinCapacity = 4;

  // We use the value 255 to indicate kNotFound for chain and bucket
  // values, which means that this value can't be used a valid
  // index.
  static const int kMaxCapacity = 254;
  STATIC_ASSERT(kMaxCapacity < kNotFound);

  static const int kNumberOfElementsOffset = 0;
  static const int kNumberOfDeletedElementsOffset = 1;
  static const int kNumberOfBucketsOffset = 2;
  static const int kBucketsStartOffset = 3;

  // The load factor is used to derive the number of buckets from
  // capacity during Allocation. We also depend on this to calaculate
  // the capacity from number of buckets after allocation. If we
  // decide to change kLoadFactor to something other than 2, capacity
  // should be stored as another field of this object.
  static const int kLoadFactor = 2;
  static const int kBitsPerPointer = kPointerSize * kBitsPerByte;

  // Our growth strategy involves doubling the capacity until we reach
  // kMaxCapacity, but since the kMaxCapacity is always less than 256,
  // we will never fully utilize this table. We special case for 256,
  // by changing the new capacity to be kMaxCapacity in
  // SmallOrderedHashTable::Grow.
  static const int kGrowthHack = 256;

  DECL_VERIFIER(SmallOrderedHashTable)

 protected:
  // This is used for accessing the non |DataTable| part of the
  // structure.
  byte get(int index) const {
    return READ_BYTE_FIELD(this, kHeaderSize + (index * kOneByteSize));
  }

  void set(int index, byte value) {
    WRITE_BYTE_FIELD(this, kHeaderSize + (index * kOneByteSize), value);
  }

  int GetDataEntryOffset(int entry, int relative_index) const {
    int datatable_start = GetDataTableStartOffset();
    int offset_in_datatable = entry * Derived::kEntrySize * kPointerSize;
    int offset_in_entry = relative_index * kPointerSize;
    return datatable_start + offset_in_datatable + offset_in_entry;
  }

  // Returns the number elements that can fit into the allocated buffer.
  int Capacity() const { return NumberOfBuckets() * kLoadFactor; }

  int UsedCapacity() const {
    return NumberOfElements() + NumberOfDeletedElements();
  }
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
  static Handle<SmallOrderedHashSet> Add(Handle<SmallOrderedHashSet> table,
                                         Handle<Object> key);
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
  static Handle<SmallOrderedHashMap> Add(Handle<SmallOrderedHashMap> table,
                                         Handle<Object> key,
                                         Handle<Object> value);
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

#endif  // V8_OBJECTS_HASH_TABLE_H_
