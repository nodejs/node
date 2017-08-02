// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_TABLE_H_
#define V8_OBJECTS_HASH_TABLE_H_

#include "src/objects.h"

#include "src/base/compiler-specific.h"
#include "src/globals.h"

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
// The hash table class is parameterized with a Shape and a Key.
// Shape must be a class with the following interface:
//   class ExampleShape {
//    public:
//      // Tells whether key matches other.
//     static bool IsMatch(Key key, Object* other);
//     // Returns the hash value for key.
//     static uint32_t Hash(Key key);
//     // Returns the hash value for object.
//     static uint32_t HashForObject(Key key, Object* object);
//     // Convert key to an object.
//     static inline Handle<Object> AsHandle(Isolate* isolate, Key key);
//     // The prefix size indicates number of elements in the beginning
//     // of the backing storage.
//     static const int kPrefixSize = ..;
//     // The Element size indicates number of elements per entry.
//     static const int kEntrySize = ..;
//   };
// The prefix size indicates an amount of memory in the
// beginning of the backing storage that can be used for non-element
// information by subclasses.

template <typename Key>
class BaseShape {
 public:
  static const bool UsesSeed = false;
  static uint32_t Hash(Key key) { return 0; }
  static uint32_t SeededHash(Key key, uint32_t seed) {
    DCHECK(UsesSeed);
    return Hash(key);
  }
  static uint32_t HashForObject(Key key, Object* object) { return 0; }
  static uint32_t SeededHashForObject(Key key, uint32_t seed, Object* object) {
    DCHECK(UsesSeed);
    return HashForObject(key, object);
  }
  static inline Map* GetMap(Isolate* isolate);
};

class V8_EXPORT_PRIVATE HashTableBase : public NON_EXPORTED_BASE(FixedArray) {
 public:
  // Returns the number of elements in the hash table.
  inline int NumberOfElements();

  // Returns the number of deleted elements in the hash table.
  inline int NumberOfDeletedElements();

  // Returns the capacity of the hash table.
  inline int Capacity();

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

  // Tells whether k is a real key.  The hole and undefined are not allowed
  // as keys and can be used to indicate missing or deleted elements.
  inline bool IsKey(Isolate* isolate, Object* k);

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
    DCHECK(base::bits::IsPowerOfTwo32(size));
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

template <typename Derived, typename Shape, typename Key>
class HashTable : public HashTableBase {
 public:
  typedef Shape ShapeT;

  // Wrapper methods.  Defined in src/objects/hash-table-inl.h
  // to break a cycle with src/heap/heap.h
  inline uint32_t Hash(Key key);
  inline uint32_t HashForObject(Key key, Object* object);

  // Returns a new HashTable object.
  MUST_USE_RESULT static Handle<Derived> New(
      Isolate* isolate, int at_least_space_for,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY,
      PretenureFlag pretenure = NOT_TENURED);

  DECLARE_CAST(HashTable)

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Find entry for key otherwise return kNotFound.
  inline int FindEntry(Key key);
  inline int FindEntry(Isolate* isolate, Key key, int32_t hash);
  int FindEntry(Isolate* isolate, Key key);
  inline bool Has(Isolate* isolate, Key key);
  inline bool Has(Key key);

  // Rehashes the table in-place.
  void Rehash(Key key);

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

 protected:
  friend class ObjectHashTable;

  MUST_USE_RESULT static Handle<Derived> New(Isolate* isolate, int capacity,
                                             PretenureFlag pretenure);

  // Find the entry at which to insert element with the given key that
  // has the given hash value.
  uint32_t FindInsertionEntry(uint32_t hash);

  // Attempt to shrink hash table after removal of key.
  MUST_USE_RESULT static Handle<Derived> Shrink(Handle<Derived> table, Key key);

  // Ensure enough space for n additional elements.
  MUST_USE_RESULT static Handle<Derived> EnsureCapacity(
      Handle<Derived> table, int n, Key key,
      PretenureFlag pretenure = NOT_TENURED);

  // Returns true if this table has sufficient capacity for adding n elements.
  bool HasSufficientCapacityToAdd(int number_of_additional_elements);

 private:
  // Ensure that kMaxRegularCapacity yields a non-large object dictionary.
  STATIC_ASSERT(EntryToIndex(kMaxRegularCapacity) < kMaxRegularLength);
  STATIC_ASSERT(v8::base::bits::IsPowerOfTwo32(kMaxRegularCapacity));
  static const int kMaxRegularEntry = kMaxRegularCapacity / kEntrySize;
  static const int kMaxRegularIndex = EntryToIndex(kMaxRegularEntry);
  STATIC_ASSERT(OffsetOfElementAt(kMaxRegularIndex) <
                kMaxRegularHeapObjectSize);

  // Sets the capacity of the hash table.
  void SetCapacity(int capacity) {
    // To scale a computed hash code to fit within the hash table, we
    // use bit-wise AND with a mask, so the capacity must be positive
    // and non-zero.
    DCHECK(capacity > 0);
    DCHECK(capacity <= kMaxCapacity);
    set(kCapacityIndex, Smi::FromInt(capacity));
  }

  // Returns _expected_ if one of entries given by the first _probe_ probes is
  // equal to  _expected_. Otherwise, returns the entry given by the probe
  // number _probe_.
  uint32_t EntryForProbe(Key key, Object* k, int probe, uint32_t expected);

  void Swap(uint32_t entry1, uint32_t entry2, WriteBarrierMode mode);

  // Rehashes this hash-table into the new table.
  void Rehash(Handle<Derived> new_table, Key key);
};

// HashTableKey is an abstract superclass for virtual key behavior.
class HashTableKey {
 public:
  // Returns whether the other object matches this key.
  virtual bool IsMatch(Object* other) = 0;
  // Returns the hash value for this key.
  virtual uint32_t Hash() = 0;
  // Returns the hash value for object.
  virtual uint32_t HashForObject(Object* key) = 0;
  // Returns the key object for storing into the hash table.
  MUST_USE_RESULT virtual Handle<Object> AsHandle(Isolate* isolate) = 0;
  // Required.
  virtual ~HashTableKey() {}
};

class ObjectHashTableShape : public BaseShape<Handle<Object>> {
 public:
  static inline bool IsMatch(Handle<Object> key, Object* other);
  static inline uint32_t Hash(Handle<Object> key);
  static inline uint32_t HashForObject(Handle<Object> key, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;
};

// ObjectHashTable maps keys that are arbitrary objects to object values by
// using the identity hash of the key for hashing purposes.
class ObjectHashTable
    : public HashTable<ObjectHashTable, ObjectHashTableShape, Handle<Object>> {
  typedef HashTable<ObjectHashTable, ObjectHashTableShape, Handle<Object>>
      DerivedHashTable;

 public:
  DECLARE_CAST(ObjectHashTable)

  // Attempt to shrink hash table after removal of key.
  MUST_USE_RESULT static inline Handle<ObjectHashTable> Shrink(
      Handle<ObjectHashTable> table, Handle<Object> key);

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

 protected:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Object* key, Object* value);
  void RemoveEntry(int entry);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + 1;
  }
};

class ObjectHashSetShape : public ObjectHashTableShape {
 public:
  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class ObjectHashSet
    : public HashTable<ObjectHashSet, ObjectHashSetShape, Handle<Object>> {
 public:
  static Handle<ObjectHashSet> Add(Handle<ObjectHashSet> set,
                                   Handle<Object> key);

  inline bool Has(Isolate* isolate, Handle<Object> key, int32_t hash);
  inline bool Has(Isolate* isolate, Handle<Object> key);

  DECLARE_CAST(ObjectHashSet)
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
class OrderedHashTable : public FixedArray {
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

  // Returns a true if the OrderedHashTable contains the key
  static bool HasKey(Handle<Derived> table, Handle<Object> key);

  int NumberOfElements() {
    return Smi::cast(get(kNumberOfElementsIndex))->value();
  }

  int NumberOfDeletedElements() {
    return Smi::cast(get(kNumberOfDeletedElementsIndex))->value();
  }

  // Returns the number of contiguous entries in the data table, starting at 0,
  // that either are real entries or have been deleted.
  int UsedCapacity() { return NumberOfElements() + NumberOfDeletedElements(); }

  int NumberOfBuckets() {
    return Smi::cast(get(kNumberOfBucketsIndex))->value();
  }

  // Returns an index into |this| for the given entry.
  int EntryToIndex(int entry) {
    return kHashTableStartIndex + NumberOfBuckets() + (entry * kEntrySize);
  }

  int HashToBucket(int hash) { return hash & (NumberOfBuckets() - 1); }

  int HashToEntry(int hash) {
    int bucket = HashToBucket(hash);
    Object* entry = this->get(kHashTableStartIndex + bucket);
    return Smi::cast(entry)->value();
  }

  int KeyToFirstEntry(Isolate* isolate, Object* key) {
    Object* hash = key->GetHash();
    // If the object does not have an identity hash, it was never used as a key
    if (hash->IsUndefined(isolate)) return kNotFound;
    return HashToEntry(Smi::cast(hash)->value());
  }

  int NextChainEntry(int entry) {
    Object* next_entry = get(EntryToIndex(entry) + kChainOffset);
    return Smi::cast(next_entry)->value();
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
    return Smi::cast(get(kRemovedHolesIndex + index))->value();
  }

  static const int kNotFound = -1;
  static const int kMinCapacity = 4;

  static const int kNumberOfElementsIndex = 0;
  // The next table is stored at the same index as the nof elements.
  static const int kNextTableIndex = kNumberOfElementsIndex;
  static const int kNumberOfDeletedElementsIndex = kNumberOfElementsIndex + 1;
  static const int kNumberOfBucketsIndex = kNumberOfDeletedElementsIndex + 1;
  static const int kHashTableStartIndex = kNumberOfBucketsIndex + 1;

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

  static const int kEntrySize = entrysize + 1;
  static const int kChainOffset = entrysize;

  static const int kLoadFactor = 2;

  // NumberOfDeletedElements is set to kClearedTableSentinel when
  // the table is cleared, which allows iterator transitions to
  // optimize that case.
  static const int kClearedTableSentinel = -1;

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

  static const int kRemovedHolesIndex = kHashTableStartIndex;

  static const int kMaxCapacity =
      (FixedArray::kMaxLength - kHashTableStartIndex) /
      (1 + (kEntrySize * kLoadFactor));
};

class OrderedHashSet : public OrderedHashTable<OrderedHashSet, 1> {
 public:
  DECLARE_CAST(OrderedHashSet)

  static Handle<OrderedHashSet> Add(Handle<OrderedHashSet> table,
                                    Handle<Object> value);
  static Handle<FixedArray> ConvertToKeysArray(Handle<OrderedHashSet> table,
                                               GetKeysConversion convert);
};

class OrderedHashMap : public OrderedHashTable<OrderedHashMap, 2> {
 public:
  DECLARE_CAST(OrderedHashMap)

  inline Object* ValueAt(int entry);

  static const int kValueOffset = 1;
};

template <int entrysize>
class WeakHashTableShape : public BaseShape<Handle<Object>> {
 public:
  static inline bool IsMatch(Handle<Object> key, Object* other);
  static inline uint32_t Hash(Handle<Object> key);
  static inline uint32_t HashForObject(Handle<Object> key, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntrySize = entrysize;
};

// WeakHashTable maps keys that are arbitrary heap objects to heap object
// values. The table wraps the keys in weak cells and store values directly.
// Thus it references keys weakly and values strongly.
class WeakHashTable
    : public HashTable<WeakHashTable, WeakHashTableShape<2>, Handle<Object>> {
  typedef HashTable<WeakHashTable, WeakHashTableShape<2>, Handle<Object>>
      DerivedHashTable;

 public:
  DECLARE_CAST(WeakHashTable)

  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object* Lookup(Handle<HeapObject> key);

  // Adds (or overwrites) the value associated with the given key. Mapping a
  // key to the hole value causes removal of the whole entry.
  MUST_USE_RESULT static Handle<WeakHashTable> Put(Handle<WeakHashTable> table,
                                                   Handle<HeapObject> key,
                                                   Handle<HeapObject> value);

  static Handle<FixedArray> GetValues(Handle<WeakHashTable> table);

 private:
  friend class MarkCompactCollector;

  void AddEntry(int entry, Handle<WeakCell> key, Handle<HeapObject> value);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return EntryToIndex(entry) + 1;
  }
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
class OrderedHashTableIterator : public JSObject {
 public:
  // [table]: the backing hash table mapping keys to values.
  DECL_ACCESSORS(table, Object)

  // [index]: The index into the data table.
  DECL_ACCESSORS(index, Object)

  // [kind]: The kind of iteration this is. One of the [Kind] enum values.
  DECL_ACCESSORS(kind, Object)

#ifdef OBJECT_PRINT
  void OrderedHashTableIteratorPrint(std::ostream& os);  // NOLINT
#endif

  static const int kTableOffset = JSObject::kHeaderSize;
  static const int kIndexOffset = kTableOffset + kPointerSize;
  static const int kKindOffset = kIndexOffset + kPointerSize;
  static const int kSize = kKindOffset + kPointerSize;

  enum Kind { kKindKeys = 1, kKindValues = 2, kKindEntries = 3 };

  // Whether the iterator has more elements. This needs to be called before
  // calling |CurrentKey| and/or |CurrentValue|.
  bool HasMore();

  // Move the index forward one.
  void MoveNext() { set_index(Smi::FromInt(Smi::cast(index())->value() + 1)); }

  // Populates the array with the next key and value and then moves the iterator
  // forward.
  // This returns the |kind| or 0 if the iterator is already at the end.
  Smi* Next(JSArray* value_array);

  // Returns the current key of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Object* CurrentKey();

 private:
  // Transitions the iterator to the non obsolete backing store. This is a NOP
  // if the [table] is not obsolete.
  void Transition();

  DISALLOW_IMPLICIT_CONSTRUCTORS(OrderedHashTableIterator);
};

class JSSetIterator
    : public OrderedHashTableIterator<JSSetIterator, OrderedHashSet> {
 public:
  // Dispatched behavior.
  DECLARE_PRINTER(JSSetIterator)
  DECLARE_VERIFIER(JSSetIterator)

  DECLARE_CAST(JSSetIterator)

  // Called by |Next| to populate the array. This allows the subclasses to
  // populate the array differently.
  inline void PopulateValueArray(FixedArray* array);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSetIterator);
};

class JSMapIterator
    : public OrderedHashTableIterator<JSMapIterator, OrderedHashMap> {
 public:
  // Dispatched behavior.
  DECLARE_PRINTER(JSMapIterator)
  DECLARE_VERIFIER(JSMapIterator)

  DECLARE_CAST(JSMapIterator)

  // Called by |Next| to populate the array. This allows the subclasses to
  // populate the array differently.
  inline void PopulateValueArray(FixedArray* array);

 private:
  // Returns the current value of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Object* CurrentValue();

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSMapIterator);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HASH_TABLE_H_
