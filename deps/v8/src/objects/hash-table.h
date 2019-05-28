// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_TABLE_H_
#define V8_OBJECTS_HASH_TABLE_H_

#include "src/base/compiler-specific.h"
#include "src/base/export-template.h"
#include "src/base/macros.h"
#include "src/globals.h"
#include "src/objects/fixed-array.h"
#include "src/objects/smi.h"
#include "src/roots.h"

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
//     static bool IsMatch(Key key, Object other);
//     // Returns the hash value for key.
//     static uint32_t Hash(Isolate* isolate, Key key);
//     // Returns the hash value for object.
//     static uint32_t HashForObject(ReadOnlyRoots roots, Object object);
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
  using Key = KeyT;
  static inline RootIndex GetMapRootIndex();
  static const bool kNeedsHoleCheck = true;
  static Object Unwrap(Object key) { return key; }
  static inline bool IsKey(ReadOnlyRoots roots, Object key);
  static inline bool IsLive(ReadOnlyRoots roots, Object key);
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
  V8_INLINE static uint32_t GetProbeOffset(uint32_t n) {
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

  OBJECT_CONSTRUCTORS(HashTableBase, FixedArray);
};

template <typename Derived, typename Shape>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) HashTable
    : public HashTableBase {
 public:
  using ShapeT = Shape;
  using Key = typename Shape::Key;

  // Returns a new HashTable object.
  V8_WARN_UNUSED_RESULT static Handle<Derived> New(
      Isolate* isolate, int at_least_space_for,
      AllocationType allocation = AllocationType::kYoung,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Find entry for key otherwise return kNotFound.
  inline int FindEntry(ReadOnlyRoots roots, Key key, int32_t hash);
  int FindEntry(Isolate* isolate, Key key);

  // Rehashes the table in-place.
  void Rehash(ReadOnlyRoots roots);

  // Tells whether k is a real key.  The hole and undefined are not allowed
  // as keys and can be used to indicate missing or deleted elements.
  static bool IsKey(ReadOnlyRoots roots, Object k);

  inline bool ToKey(ReadOnlyRoots roots, int entry, Object* out_k);

  // Returns the key at entry.
  Object KeyAt(int entry) { return get(EntryToIndex(entry) + kEntryKeyIndex); }

  static const int kElementsStartIndex = kPrefixStartIndex + Shape::kPrefixSize;
  static const int kEntrySize = Shape::kEntrySize;
  STATIC_ASSERT(kEntrySize > 0);
  static const int kEntryKeyIndex = 0;
  static const int kElementsStartOffset =
      kHeaderSize + kElementsStartIndex * kTaggedSize;
  // Maximal capacity of HashTable. Based on maximal length of underlying
  // FixedArray. Staying below kMaxCapacity also ensures that EntryToIndex
  // cannot overflow.
  static const int kMaxCapacity =
      (FixedArray::kMaxLength - kElementsStartIndex) / kEntrySize;

  // Don't shrink a HashTable below this capacity.
  static const int kMinShrinkCapacity = 16;

  static const int kMaxRegularCapacity = kMaxRegularHeapObjectSize / 32;

  // Returns the index for an entry (of the key)
  static constexpr inline int EntryToIndex(int entry) {
    return (entry * kEntrySize) + kElementsStartIndex;
  }

  // Returns the index for an entry (of the key)
  static constexpr inline int IndexToEntry(int index) {
    return (index - kElementsStartIndex) / kEntrySize;
  }

  // Returns the index for a slot address in the object.
  static constexpr inline int SlotToIndex(Address object, Address slot) {
    return static_cast<int>((slot - object - kHeaderSize) / kTaggedSize);
  }

  // Ensure enough space for n additional elements.
  V8_WARN_UNUSED_RESULT static Handle<Derived> EnsureCapacity(
      Isolate* isolate, Handle<Derived> table, int n,
      AllocationType allocation = AllocationType::kYoung);

  // Returns true if this table has sufficient capacity for adding n elements.
  bool HasSufficientCapacityToAdd(int number_of_additional_elements);

 protected:
  friend class ObjectHashTable;

  V8_WARN_UNUSED_RESULT static Handle<Derived> NewInternal(
      Isolate* isolate, int capacity, AllocationType allocation);

  // Find the entry at which to insert element with the given key that
  // has the given hash value.
  uint32_t FindInsertionEntry(uint32_t hash);

  // Attempt to shrink hash table after removal of key.
  V8_WARN_UNUSED_RESULT static Handle<Derived> Shrink(
      Isolate* isolate, Handle<Derived> table, int additionalCapacity = 0);

  inline void set_key(int index, Object value);
  inline void set_key(int index, Object value, WriteBarrierMode mode);

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
  uint32_t EntryForProbe(ReadOnlyRoots roots, Object k, int probe,
                         uint32_t expected);

  void Swap(uint32_t entry1, uint32_t entry2, WriteBarrierMode mode);

  // Rehashes this hash-table into the new table.
  void Rehash(ReadOnlyRoots roots, Derived new_table);

  OBJECT_CONSTRUCTORS(HashTable, HashTableBase);
};

// HashTableKey is an abstract superclass for virtual key behavior.
class HashTableKey {
 public:
  explicit HashTableKey(uint32_t hash) : hash_(hash) {}

  // Returns whether the other object matches this key.
  virtual bool IsMatch(Object other) = 0;
  // Returns the hash value for this key.
  // Required.
  virtual ~HashTableKey() = default;

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
  static inline bool IsMatch(Handle<Object> key, Object other);
  static inline uint32_t Hash(Isolate* isolate, Handle<Object> key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots, Object object);
  static inline Handle<Object> AsHandle(Handle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntryValueIndex = 1;
  static const int kEntrySize = 2;
  static const bool kNeedsHoleCheck = false;
};

template <typename Derived, typename Shape>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) ObjectHashTableBase
    : public HashTable<Derived, Shape> {
 public:
  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object Lookup(Handle<Object> key);
  Object Lookup(Handle<Object> key, int32_t hash);
  Object Lookup(ReadOnlyRoots roots, Handle<Object> key, int32_t hash);

  // Returns the value at entry.
  Object ValueAt(int entry);

  // Overwrite all keys and values with the hole value.
  static void FillEntriesWithHoles(Handle<Derived>);

  // Adds (or overwrites) the value associated with the given key.
  static Handle<Derived> Put(Handle<Derived> table, Handle<Object> key,
                             Handle<Object> value);
  static Handle<Derived> Put(Isolate* isolate, Handle<Derived> table,
                             Handle<Object> key, Handle<Object> value,
                             int32_t hash);

  // Returns an ObjectHashTable (possibly |table|) where |key| has been removed.
  static Handle<Derived> Remove(Isolate* isolate, Handle<Derived> table,
                                Handle<Object> key, bool* was_present);
  static Handle<Derived> Remove(Isolate* isolate, Handle<Derived> table,
                                Handle<Object> key, bool* was_present,
                                int32_t hash);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(int entry) {
    return HashTable<Derived, Shape>::EntryToIndex(entry) +
           Shape::kEntryValueIndex;
  }

 protected:
  void AddEntry(int entry, Object key, Object value);
  void RemoveEntry(int entry);

  OBJECT_CONSTRUCTORS(ObjectHashTableBase, HashTable<Derived, Shape>);
};

class ObjectHashTable;

extern template class EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) HashTable<ObjectHashTable, ObjectHashTableShape>;
extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    ObjectHashTableBase<ObjectHashTable, ObjectHashTableShape>;

// ObjectHashTable maps keys that are arbitrary objects to object values by
// using the identity hash of the key for hashing purposes.
class V8_EXPORT_PRIVATE ObjectHashTable
    : public ObjectHashTableBase<ObjectHashTable, ObjectHashTableShape> {
 public:
  DECL_CAST(ObjectHashTable)
  DECL_PRINTER(ObjectHashTable)

  OBJECT_CONSTRUCTORS(
      ObjectHashTable,
      ObjectHashTableBase<ObjectHashTable, ObjectHashTableShape>);
};

class EphemeronHashTableShape : public ObjectHashTableShape {
 public:
  static inline RootIndex GetMapRootIndex();
};

class EphemeronHashTable;

extern template class EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) HashTable<EphemeronHashTable, EphemeronHashTableShape>;
extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape>;

// EphemeronHashTable is similar to ObjectHashTable but gets special treatment
// by the GC. The GC treats its entries as ephemerons: both key and value are
// weak references, however if the key is strongly reachable its corresponding
// value is also kept alive.
class V8_EXPORT_PRIVATE EphemeronHashTable
    : public ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape> {
 public:
  DECL_CAST(EphemeronHashTable)
  DECL_PRINTER(EphemeronHashTable)
  class BodyDescriptor;

 protected:
  friend class MarkCompactCollector;
  friend class ScavengerCollector;
  friend class HashTable<EphemeronHashTable, EphemeronHashTableShape>;
  friend class ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape>;
  inline void set_key(int index, Object value);
  inline void set_key(int index, Object value, WriteBarrierMode mode);

  OBJECT_CONSTRUCTORS(
      EphemeronHashTable,
      ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape>);
};

class ObjectHashSetShape : public ObjectHashTableShape {
 public:
  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

class ObjectHashSet;
extern template class EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) HashTable<ObjectHashSet, ObjectHashSetShape>;

class V8_EXPORT_PRIVATE ObjectHashSet
    : public HashTable<ObjectHashSet, ObjectHashSetShape> {
 public:
  static Handle<ObjectHashSet> Add(Isolate* isolate, Handle<ObjectHashSet> set,
                                   Handle<Object> key);

  inline bool Has(Isolate* isolate, Handle<Object> key, int32_t hash);
  inline bool Has(Isolate* isolate, Handle<Object> key);

  DECL_CAST(ObjectHashSet)

  OBJECT_CONSTRUCTORS(ObjectHashSet,
                      HashTable<ObjectHashSet, ObjectHashSetShape>);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HASH_TABLE_H_
