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
#include "src/objects/tagged-field.h"
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
//     static bool IsMatch(Key key, Tagged<Object> other);
//     // Returns the hash value for key.
//     static uint32_t Hash(ReadOnlyRoots roots, Key key);
//     // Returns the hash value for object.
//     static uint32_t HashForObject(ReadOnlyRoots roots,
//                                   Tagged<Object> object);
//     // Convert key to an object.
//     static inline DirectHandle<Object> AsHandle(Isolate* isolate, Key key);
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
  static Tagged<Object> Unwrap(Tagged<Object> key) { return key; }
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

  // Set the number of elements in the hash table after a bulk of elements was
  // added.
  inline void SetInitialNumberOfElements(int nof);

 protected:
  // Update the number of elements in the hash table.
  inline void SetNumberOfElements(int nof);

  // Update the number of deleted elements in the hash table.
  inline void SetNumberOfDeletedElements(int nod);

  inline static InternalIndex NextProbe(InternalIndex last, uint32_t number,
                                        uint32_t size) {
    return InternalIndex((last.as_uint32() + number) & (size - 1));
  }
};

template <typename Derived, typename ShapeT>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) HashTable
    : public HashTableBase {
 public:
  // TODO(jgruber): Derive from TaggedArrayBase instead of FixedArray, and
  // merge with TaggedArrayBase's Shape class. Once the naming conflict is
  // resolved rename all TodoShape occurrences back to Shape.
  using TodoShape = ShapeT;
  using Key = typename TodoShape::Key;

  // Returns a new HashTable object.
  template <typename IsolateT>
  V8_WARN_UNUSED_RESULT static Handle<Derived> New(
      IsolateT* isolate, int at_least_space_for,
      AllocationType allocation = AllocationType::kYoung,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  static inline DirectHandle<Map> GetMap(RootsTable& roots);

  // Garbage collection support.
  void IteratePrefix(ObjectVisitor* visitor);
  void IterateElements(ObjectVisitor* visitor);

  // Returns probe entry.
  inline static InternalIndex FirstProbe(uint32_t hash, uint32_t size) {
    if (!TodoShape::kDoHashSpreading || size <= (1 << TodoShape::kHashBits)) {
      return InternalIndex(hash & (size - 1));
    }
    // If the hash only has N bits and size is large, the hashes will all be
    // clustered at the beginning of the hash table. This distributes the
    // items more evenly and makes the lookup chains shorter.
    DCHECK_GE(TodoShape::kHashBits, 1);
    DCHECK_LE(TodoShape::kHashBits, 32);
    uint32_t coefficient = size >> TodoShape::kHashBits;
    DCHECK_GE(coefficient, 2);
    return InternalIndex((hash * coefficient) & (size - 1));
  }

  // Find entry for key otherwise return kNotFound.
  inline InternalIndex FindEntry(PtrComprCageBase cage_base,
                                 ReadOnlyRoots roots, Key key, int32_t hash);
  template <typename IsolateT>
  inline InternalIndex FindEntry(IsolateT* isolate, Key key);

  // Rehashes the table in-place.
  void Rehash(PtrComprCageBase cage_base);

  // Returns whether k is a real key.  The hole and undefined are not allowed as
  // keys and can be used to indicate missing or deleted elements.
  static inline bool IsKey(ReadOnlyRoots roots, Tagged<Object> k);

  inline bool ToKey(ReadOnlyRoots roots, InternalIndex entry,
                    Tagged<Object>* out_k);
  inline bool ToKey(PtrComprCageBase cage_base, InternalIndex entry,
                    Tagged<Object>* out_k);

  // Returns the key at entry.
  inline Tagged<Object> KeyAt(InternalIndex entry);
  inline Tagged<Object> KeyAt(PtrComprCageBase cage_base, InternalIndex entry);
  inline Tagged<Object> KeyAt(InternalIndex entry, RelaxedLoadTag tag);
  inline Tagged<Object> KeyAt(PtrComprCageBase cage_base, InternalIndex entry,
                              RelaxedLoadTag tag);

  inline void SetKeyAt(InternalIndex entry, Tagged<Object> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static const int kElementsStartIndex =
      kPrefixStartIndex + TodoShape::kPrefixSize;
  static const int kEntrySize = TodoShape::kEntrySize;
  static_assert(kEntrySize > 0);
  static const int kEntryKeyIndex = 0;
  static const int kElementsStartOffset =
      OFFSET_OF_DATA_START(HashTableBase) + kElementsStartIndex * kTaggedSize;
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
    return static_cast<int>((slot - object - sizeof(HashTableBase)) /
                            kTaggedSize);
  }

  // Ensure enough space for n additional elements.
  template <typename IsolateT, template <typename> typename HandleType>
    requires(std::is_convertible_v<HandleType<Derived>, DirectHandle<Derived>>)
  V8_WARN_UNUSED_RESULT static HandleType<Derived> EnsureCapacity(
      IsolateT* isolate, HandleType<Derived> table, int n = 1,
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
  template <typename IsolateT>
  InternalIndex FindInsertionEntry(IsolateT* isolate, uint32_t hash);

  // Computes the capacity a table with the given capacity would need to have
  // room for the given number of elements, also allowing it to shrink.
  static int ComputeCapacityWithShrink(int current_capacity,
                                       int at_least_room_for);

  // Shrink the hash table.
  template <template <typename> typename HandleType>
    requires(std::is_convertible_v<HandleType<Derived>, DirectHandle<Derived>>)
  V8_WARN_UNUSED_RESULT static HandleType<Derived> Shrink(
      Isolate* isolate, HandleType<Derived> table, int additionalCapacity = 0);

  // Rehashes this hash-table into the new table.
  void Rehash(PtrComprCageBase cage_base, Tagged<Derived> new_table);

  inline void set_key(int index, Tagged<Object> value);
  inline void set_key(int index, Tagged<Object> value, WriteBarrierMode mode);

 private:
  // Ensure that kMaxRegularCapacity yields a non-large object dictionary.
  static_assert(EntryToIndex(InternalIndex(kMaxRegularCapacity)) <
                FixedArray::kMaxRegularLength);
  static_assert(v8::base::bits::IsPowerOfTwo(kMaxRegularCapacity));
  static const int kMaxRegularEntry = kMaxRegularCapacity / kEntrySize;
  static const int kMaxRegularIndex =
      EntryToIndex(InternalIndex(kMaxRegularEntry));
  static_assert(OffsetOfElementAt(kMaxRegularIndex) <
                kMaxRegularHeapObjectSize);

  // Sets the capacity of the hash table.
  inline void SetCapacity(int capacity);

  // Returns _expected_ if one of entries given by the first _probe_ probes is
  // equal to  _expected_. Otherwise, returns the entry given by the probe
  // number _probe_.
  InternalIndex EntryForProbe(ReadOnlyRoots roots, Tagged<Object> k, int probe,
                              InternalIndex expected);

  void Swap(InternalIndex entry1, InternalIndex entry2, WriteBarrierMode mode);
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
                                            int, AllocationType);            \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)                 \
      DirectHandle<DERIVED>                                                  \
      HashTable<DERIVED, SHAPE>::EnsureCapacity(                             \
          Isolate*, DirectHandle<DERIVED>, int, AllocationType);             \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)                 \
      DirectHandle<DERIVED>                                                  \
      HashTable<DERIVED, SHAPE>::EnsureCapacity(                             \
          LocalIsolate*, DirectHandle<DERIVED>, int, AllocationType);        \
                                                                             \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) Handle<DERIVED> \
  HashTable<DERIVED, SHAPE>::Shrink(Isolate*, Handle<DERIVED>, int);         \
  extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)                 \
      DirectHandle<DERIVED>                                                  \
      HashTable<DERIVED, SHAPE>::Shrink(Isolate*, DirectHandle<DERIVED>, int);

// HashTableKey is an abstract superclass for virtual key behavior.
class HashTableKey {
 public:
  explicit HashTableKey(uint32_t hash) : hash_(hash) {}

  // Returns whether the other object matches this key.
  virtual bool IsMatch(Tagged<Object> other) = 0;
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

class ObjectHashTableShapeBase : public BaseShape<DirectHandle<Object>> {
 public:
  static inline bool IsMatch(DirectHandle<Object> key, Tagged<Object> other);
  static inline uint32_t Hash(ReadOnlyRoots roots, DirectHandle<Object> key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots,
                                       Tagged<Object> object);
  static inline DirectHandle<Object> AsHandle(DirectHandle<Object> key);
  static const int kPrefixSize = 0;
  static const int kEntryValueIndex = 1;
  static const int kEntrySize = 2;
  static const bool kMatchNeedsHoleCheck = false;
};

class ObjectHashTableShape : public ObjectHashTableShapeBase {
 public:
  // TODO(marja): Investigate whether adding hash spreading here would help
  // other use cases.
  static const bool kDoHashSpreading = false;
  static const uint32_t kHashBits = 0;
};

class EphemeronHashTableShape : public ObjectHashTableShapeBase {
 public:
  // To support large WeakMaps, spread the probes more evenly if the hash table
  // is big and we don't have enough hash bits to cover it all. This needs to be
  // in sync with code generated by WeakCollectionsBuiltinsAssembler.
  static const bool kDoHashSpreading = true;
  static const uint32_t kHashBits;
};

template <typename Derived, typename Shape>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) ObjectHashTableBase
    : public HashTable<Derived, Shape> {
 public:
  // Looks up the value associated with the given key. The hole value is
  // returned in case the key is not present.
  Tagged<Object> Lookup(DirectHandle<Object> key);
  Tagged<Object> Lookup(DirectHandle<Object> key, int32_t hash);
  Tagged<Object> Lookup(PtrComprCageBase cage_base, DirectHandle<Object> key,
                        int32_t hash);

  // Returns the value at entry.
  Tagged<Object> ValueAt(InternalIndex entry);

  // Overwrite all keys and values with the hole value.
  static void FillEntriesWithHoles(DirectHandle<Derived>);

  // Adds (or overwrites) the value associated with the given key.
  static Handle<Derived> Put(Handle<Derived> table, DirectHandle<Object> key,
                             DirectHandle<Object> value);
  static Handle<Derived> Put(Isolate* isolate, Handle<Derived> table,
                             DirectHandle<Object> key,
                             DirectHandle<Object> value, int32_t hash);

  // Returns an ObjectHashTable (possibly |table|) where |key| has been removed.
  static Handle<Derived> Remove(Isolate* isolate, Handle<Derived> table,
                                DirectHandle<Object> key, bool* was_present);
  static Handle<Derived> Remove(Isolate* isolate, Handle<Derived> table,
                                DirectHandle<Object> key, bool* was_present,
                                int32_t hash);

  // Returns the index to the value of an entry.
  static inline int EntryToValueIndex(InternalIndex entry) {
    return HashTable<Derived, Shape>::EntryToIndex(entry) +
           Shape::kEntryValueIndex;
  }

 protected:
  void AddEntry(InternalIndex entry, Tagged<Object> key, Tagged<Object> value);
  void RemoveEntry(InternalIndex entry);
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
  DECL_PRINTER(ObjectHashTable)
};

EXTERN_DECLARE_OBJECT_BASE_HASH_TABLE(EphemeronHashTable,
                                      EphemeronHashTableShape)

// EphemeronHashTable is similar to ObjectHashTable but gets special treatment
// by the GC. The GC treats its entries as ephemerons: both key and value are
// weak references, however if the key is strongly reachable its corresponding
// value is also kept alive.
class V8_EXPORT_PRIVATE EphemeronHashTable
    : public ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape> {
 public:
  static inline Handle<Map> GetMap(RootsTable& roots);

  DECL_PRINTER(EphemeronHashTable)
  class BodyDescriptor;

 protected:
  friend class MarkCompactCollector;
  friend class MinorMarkSweepCollector;
  friend class ScavengerCollector;
  friend class HashTable<EphemeronHashTable, EphemeronHashTableShape>;
  friend class ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape>;
  inline void set_key(int index, Tagged<Object> value);
  inline void set_key(int index, Tagged<Object> value, WriteBarrierMode mode);
};

// ObjectMultihashTable is a hash table that maps Object keys to N Object
// values. The Object values are stored inline in the underlying FixedArray.
//
// This is not a generic multimap where each key can map to a variable number of
// values. Each key always maps to exactly N values.
template <int N>
class ObjectMultiHashTableShape : public ObjectHashTableShape {
 public:
  static const int kEntrySize = 1 + N;
};

template <typename Derived, int N>
class ObjectMultiHashTableBase
    : public HashTable<Derived, ObjectMultiHashTableShape<N>> {
 public:
  static_assert(N > 1, "use ObjectHashTable instead if N = 1");

  // Returns the values associated with the given key. Return an std::array of
  // holes if not found.
  std::array<Tagged<Object>, N> Lookup(DirectHandle<Object> key);
  std::array<Tagged<Object>, N> Lookup(PtrComprCageBase cage_base,
                                       DirectHandle<Object> key);

  // Adds or overwrites the values associated with the given key.
  static Handle<Derived> Put(Isolate* isolate, Handle<Derived> table,
                             DirectHandle<Object> key,
                             const std::array<DirectHandle<Object>, N>& values);

 private:
  void SetEntryValues(InternalIndex entry,
                      const std::array<DirectHandle<Object>, N>& values);

  static constexpr inline int EntryToValueIndexStart(InternalIndex entry) {
    return HashTable<Derived, ObjectMultiHashTableShape<N>>::EntryToIndex(
               entry) +
           ObjectMultiHashTableShape<N>::kEntryValueIndex;
  }
};

class ObjectTwoHashTable
    : public ObjectMultiHashTableBase<ObjectTwoHashTable, 2> {
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
                                   DirectHandle<Object> key);

  inline bool Has(Isolate* isolate, DirectHandle<Object> key, int32_t hash);
  inline bool Has(Isolate* isolate, DirectHandle<Object> key);
};

class NameToIndexShape : public BaseShape<DirectHandle<Name>> {
 public:
  static inline bool IsMatch(DirectHandle<Name> key, Tagged<Object> other);
  static inline uint32_t Hash(ReadOnlyRoots roots, DirectHandle<Name> key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots,
                                       Tagged<Object> object);
  static inline DirectHandle<Object> AsHandle(DirectHandle<Name> key);
  static const int kPrefixSize = 0;
  static const int kEntryValueIndex = 1;
  static const int kEntrySize = 2;
  static const bool kMatchNeedsHoleCheck = false;
  static const bool kDoHashSpreading = false;
  static const uint32_t kHashBits = 0;
};

class V8_EXPORT_PRIVATE NameToIndexHashTable
    : public HashTable<NameToIndexHashTable, NameToIndexShape> {
 public:
  static const int kEntryValueIndex = NameToIndexShape::kEntryValueIndex;

  inline static DirectHandle<Map> GetMap(RootsTable& roots);
  int Lookup(DirectHandle<Name> key);

  // Returns the value at entry.
  Tagged<Object> ValueAt(InternalIndex entry);
  int IndexAt(InternalIndex entry);

  template <typename IsolateT>
  static Handle<NameToIndexHashTable> Add(IsolateT* isolate,
                                          Handle<NameToIndexHashTable> table,
                                          DirectHandle<Name> key,
                                          int32_t value);

  // Exposed for NameDictionaryLookupForwardedString slow path for forwarded
  // strings.
  using HashTable<NameToIndexHashTable, NameToIndexShape>::FindInsertionEntry;

  DECL_PRINTER(NameToIndexHashTable)

 private:
  static inline int EntryToValueIndex(InternalIndex entry) {
    return EntryToIndex(entry) + NameToIndexShape::kEntryValueIndex;
  }
};

class RegisteredSymbolTableShape : public BaseShape<DirectHandle<String>> {
 public:
  static inline bool IsMatch(DirectHandle<String> key, Tagged<Object> other);
  static inline uint32_t Hash(ReadOnlyRoots roots, DirectHandle<String> key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots,
                                       Tagged<Object> object);
  static const int kPrefixSize = 0;
  static const int kEntryValueIndex = 1;
  static const int kEntrySize = 2;
  static const bool kMatchNeedsHoleCheck = false;
  static const bool kDoHashSpreading = false;
  static const uint32_t kHashBits = 0;
};

class RegisteredSymbolTable
    : public HashTable<RegisteredSymbolTable, RegisteredSymbolTableShape> {
 public:
  Tagged<Object> SlowReverseLookup(Tagged<Object> value);

  // Returns the value at entry.
  Tagged<Object> ValueAt(InternalIndex entry);

  inline static DirectHandle<Map> GetMap(RootsTable& roots);

  static Handle<RegisteredSymbolTable> Add(Isolate* isolate,
                                           Handle<RegisteredSymbolTable> table,
                                           DirectHandle<String> key,
                                           DirectHandle<Symbol>);

  DECL_PRINTER(RegisteredSymbolTable)

 private:
  static inline int EntryToValueIndex(InternalIndex entry) {
    return EntryToIndex(entry) + RegisteredSymbolTableShape::kEntryValueIndex;
  }
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HASH_TABLE_H_
