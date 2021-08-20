// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_TABLE_H_
#define V8_OBJECTS_HASH_TABLE_H_

#include "src/base/compiler-specific.h"
#include "src/base/export-template.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/execution/isolate-utils.h"
#include "src/objects/fixed-array.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"

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
//     static uint32_t Hash(ReadOnlyRoots roots, Key key);
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
//     static const bool kMatchNeedsHoleCheck = ..;
//   };
// The prefix size indicates an amount of memory in the
// beginning of the backing storage that can be used for non-element
// information by subclasses.

template <typename KeyT>
class V8_EXPORT_PRIVATE BaseShape {
 public:
  using Key = KeyT;
  static Object Unwrap(Object key) { return key; }
};

class V8_EXPORT_PRIVATE HashTableBase : public NON_EXPORTED_BASE(FixedArray) {
 public:
  // Returns the number of elements in the hash table.
  inline int NumberOfElements() const;

  // Returns the number of deleted elements in the hash table.
  inline int NumberOfDeletedElements() const;

  // Returns the capacity of the hash table.
  inline int Capacity() const;

  inline InternalIndex::Range IterateEntries() const;

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

  static const int kNumberOfElementsIndex = 0;
  static const int kNumberOfDeletedElementsIndex = 1;
  static const int kCapacityIndex = 2;
  static const int kPrefixStartIndex = 3;

  // Minimum capacity for newly created hash tables.
  static const int kMinCapacity = 4;

 protected:
  // Update the number of elements in the hash table.
  inline void SetNumberOfElements(int nof);

  // Update the number of deleted elements in the hash table.
  inline void SetNumberOfDeletedElements(int nod);

  // Returns probe entry.
  inline static InternalIndex FirstProbe(uint32_t hash, uint32_t size) {
    return InternalIndex(hash & (size - 1));
  }

  inline static InternalIndex NextProbe(InternalIndex last, uint32_t number,
                                        uint32_t size) {
    return InternalIndex((last.as_uint32() + number) & (size - 1));
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
  template <typename IsolateT>
  V8_WARN_UNUSED_RESULT static Handle<Derived> New(
      IsolateT* isolate, int at_least_space_for,
      AllocationType allocation = AllocationType::kYoung,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  static inline Handle<Map> GetMap(ReadOnlyRoots roots);

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Find entry for key otherwise return kNotFound.
  inline InternalIndex FindEntry(PtrComprCageBase cage_base,
                                 ReadOnlyRoots roots, Key key, int32_t hash);
  template <typename IsolateT>
  inline InternalIndex FindEntry(IsolateT* isolate, Key key);

  // Rehashes the table in-place.
  void Rehash(PtrComprCageBase cage_base);

  // Returns whether k is a real key.  The hole and undefined are not allowed as
  // keys and can be used to indicate missing or deleted elements.
  static inline bool IsKey(ReadOnlyRoots roots, Object k);

  inline bool ToKey(ReadOnlyRoots roots, InternalIndex entry, Object* out_k);
  inline bool ToKey(PtrComprCageBase cage_base, InternalIndex entry,
                    Object* out_k);

  // Returns the key at entry.
  inline Object KeyAt(InternalIndex entry);
  inline Object KeyAt(PtrComprCageBase cage_base, InternalIndex entry);

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

  // Pretenure hashtables above this capacity.
  static const int kMinCapacityForPretenure = 256;

  static const int kMaxRegularCapacity = kMaxRegularHeapObjectSize / 32;

  // Returns the index for an entry (of the key)
  static constexpr inline int EntryToIndex(InternalIndex entry) {
    return (entry.as_int() * kEntrySize) + kElementsStartIndex;
  }

  // Returns the entry for an index (of the key)
  static constexpr inline InternalIndex IndexToEntry(int index) {
    return InternalIndex((index - kElementsStartIndex) / kEntrySize);
  }

  // Returns the index for a slot address in the object.
  static constexpr inline int SlotToIndex(Address object, Address slot) {
    return static_cast<int>((slot - object - kHeaderSize) / kTaggedSize);
  }

  // Ensure enough space for n additional elements.
  template <typename IsolateT>
  V8_WARN_UNUSED_RESULT static Handle<Derived> EnsureCapacity(
      IsolateT* isolate, Handle<Derived> table, int n = 1,
      AllocationType allocation = AllocationType::kYoung);

  // Returns true if this table has sufficient capacity for adding n elements.
  bool HasSufficientCapacityToAdd(int number_of_additional_elements);

  // Returns true if a table with the given parameters has sufficient capacity
  // for adding n elements. Can be used to check hypothetical capacities without
  // actually allocating a table with that capacity.
  static bool HasSufficientCapacityToAdd(int capacity, int number_of_elements,
                                         int number_of_deleted_elements,
                                         int number_of_additional_elements);

 protected:
  friend class ObjectHashTable;

  template <typename IsolateT>
  V8_WARN_UNUSED_RESULT static Handle<Derived> NewInternal(
      IsolateT* isolate, int capacity, AllocationType allocation);

  // Find the entry at which to insert element with the given key that
  // has the given hash value.
  InternalIndex FindInsertionEntry(PtrComprCageBase cage_base,
                                   ReadOnlyRoots roots, uint32_t hash);
  InternalIndex FindInsertionEntry(Isolate* isolate, uint32_t hash);

  // Computes the capacity a table with the given capacity would need to have
  // room for the given number of elements, also allowing it to shrink.
  static int ComputeCapacityWithShrink(int current_capacity,
                                       int at_least_room_for);

  // Shrink the hash table.
  V8_WARN_UNUSED_RESULT static Handle<Derived> Shrink(
      Isolate* isolate, Handle<Derived> table, int additionalCapacity = 0);

  // Rehashes this hash-table into the new table.
  void Rehash(PtrComprCageBase cage_base, Derived new_table);

  inline void set_key(int index, Object value);
  inline void set_key(int index, Object value, WriteBarrierMode mode);

 private:
  // Ensure that kMaxRegularCapacity yields a non-large object dictionary.
  STATIC_ASSERT(EntryToIndex(InternalIndex(kMaxRegularCapacity)) <
                kMaxRegularLength);
  STATIC_ASSERT(v8::base::bits::IsPowerOfTwo(kMaxRegularCapacity));
  static const int kMaxRegularEntry = kMaxRegularCapacity / kEntrySize;
  static const int kMaxRegularIndex =
      EntryToIndex(InternalIndex(kMaxRegularEntry));
  STATIC_ASSERT(OffsetOfElementAt(kMaxRegularIndex) <
                kMaxRegularHeapObjectSize);

  // Sets the capacity of the hash table.
  inline void SetCapacity(int capacity);

  // Returns _expected_ if one of entries given by the first _probe_ probes is
  // equal to  _expected_. Otherwise, returns the entry given by the probe
  // number _probe_.
  InternalIndex EntryForProbe(ReadOnlyRoots roots, Object k, int probe,
                              InternalIndex expected);

  void Swap(InternalIndex entry1, InternalIndex entry2, WriteBarrierMode mode);

  OBJECT_CONSTRUCTORS(HashTable, HashTableBase);
};

#define EXTERN_DECLARE_HASH_TABLE(DERIVED, SHAPE)                            \
  extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)           \
      HashTable<class DERIVED, SHAPE>;                                       \
                                                                             \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) Handle<DERIVED> \
  HashTable<DERIVED, SHAPE>::New(Isolate*, int, AllocationType,              \
                                 MinimumCapacity);                           \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) Handle<DERIVED> \
  HashTable<DERIVED, SHAPE>::New(LocalIsolate*, int, AllocationType,         \
                                 MinimumCapacity);                           \
                                                                             \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) Handle<DERIVED> \
  HashTable<DERIVED, SHAPE>::EnsureCapacity(Isolate*, Handle<DERIVED>, int,  \
                                            AllocationType);                 \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) Handle<DERIVED> \
  HashTable<DERIVED, SHAPE>::EnsureCapacity(LocalIsolate*, Handle<DERIVED>,  \
                                            int, AllocationType);

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
  static inline uint32_t Hash(ReadOnlyRoots roots, Handle<Object> key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots, Object object);
  static inline Handle<Object> AsHandle(Handle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntryValueIndex = 1;
  static const int kEntrySize = 2;
  static const bool kMatchNeedsHoleCheck = false;
};

template <typename Derived, typename Shape>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) ObjectHashTableBase
    : public HashTable<Derived, Shape> {
 public:
  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Object Lookup(Handle<Object> key);
  Object Lookup(Handle<Object> key, int32_t hash);
  Object Lookup(PtrComprCageBase cage_base, Handle<Object> key, int32_t hash);

  // Returns the value at entry.
  Object ValueAt(InternalIndex entry);

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
  static inline int EntryToValueIndex(InternalIndex entry) {
    return HashTable<Derived, Shape>::EntryToIndex(entry) +
           Shape::kEntryValueIndex;
  }

 protected:
  void AddEntry(InternalIndex entry, Object key, Object value);
  void RemoveEntry(InternalIndex entry);

  OBJECT_CONSTRUCTORS(ObjectHashTableBase, HashTable<Derived, Shape>);
};

#define EXTERN_DECLARE_OBJECT_BASE_HASH_TABLE(DERIVED, SHAPE)      \
  EXTERN_DECLARE_HASH_TABLE(DERIVED, SHAPE)                        \
  extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) \
      ObjectHashTableBase<class DERIVED, SHAPE>;

EXTERN_DECLARE_OBJECT_BASE_HASH_TABLE(ObjectHashTable, ObjectHashTableShape)

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

EXTERN_DECLARE_OBJECT_BASE_HASH_TABLE(EphemeronHashTable, ObjectHashTableShape)

// EphemeronHashTable is similar to ObjectHashTable but gets special treatment
// by the GC. The GC treats its entries as ephemerons: both key and value are
// weak references, however if the key is strongly reachable its corresponding
// value is also kept alive.
class V8_EXPORT_PRIVATE EphemeronHashTable
    : public ObjectHashTableBase<EphemeronHashTable, ObjectHashTableShape> {
 public:
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);

  DECL_CAST(EphemeronHashTable)
  DECL_PRINTER(EphemeronHashTable)
  class BodyDescriptor;

 protected:
  friend class MarkCompactCollector;
  friend class ScavengerCollector;
  friend class HashTable<EphemeronHashTable, ObjectHashTableShape>;
  friend class ObjectHashTableBase<EphemeronHashTable, ObjectHashTableShape>;
  inline void set_key(int index, Object value);
  inline void set_key(int index, Object value, WriteBarrierMode mode);

  OBJECT_CONSTRUCTORS(
      EphemeronHashTable,
      ObjectHashTableBase<EphemeronHashTable, ObjectHashTableShape>);
};

class ObjectHashSetShape : public ObjectHashTableShape {
 public:
  static const int kPrefixSize = 0;
  static const int kEntrySize = 1;
};

EXTERN_DECLARE_HASH_TABLE(ObjectHashSet, ObjectHashSetShape)

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
