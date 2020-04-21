// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DICTIONARY_H_
#define V8_OBJECTS_DICTIONARY_H_

#include "src/base/export-template.h"
#include "src/common/globals.h"
#include "src/objects/hash-table.h"
#include "src/objects/property-array.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class Isolate;

template <typename Derived, typename Shape>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) Dictionary
    : public HashTable<Derived, Shape> {
  using DerivedHashTable = HashTable<Derived, Shape>;

 public:
  using Key = typename Shape::Key;
  // Returns the value at entry.
  inline Object ValueAt(InternalIndex entry);
  inline Object ValueAt(const Isolate* isolate, InternalIndex entry);

  // Set the value for entry.
  inline void ValueAtPut(InternalIndex entry, Object value);

  // Returns the property details for the property at entry.
  inline PropertyDetails DetailsAt(InternalIndex entry);

  // Set the details for entry.
  inline void DetailsAtPut(InternalIndex entry, PropertyDetails value);

  // Delete a property from the dictionary.
  V8_WARN_UNUSED_RESULT static Handle<Derived> DeleteEntry(
      Isolate* isolate, Handle<Derived> dictionary, InternalIndex entry);

  // Attempt to shrink the dictionary after deletion of key.
  V8_WARN_UNUSED_RESULT static inline Handle<Derived> Shrink(
      Isolate* isolate, Handle<Derived> dictionary) {
    return DerivedHashTable::Shrink(isolate, dictionary);
  }

  int NumberOfEnumerableProperties();

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  void Print(std::ostream& os);  // NOLINT
#endif
  // Returns the key (slow).
  Object SlowReverseLookup(Object value);

  // Sets the entry to (key, value) pair.
  inline void ClearEntry(InternalIndex entry);
  inline void SetEntry(InternalIndex entry, Object key, Object value,
                       PropertyDetails details);

  // Garbage collection support.
  inline ObjectSlot RawFieldOfValueAt(InternalIndex entry);

  template <typename LocalIsolate>
  V8_WARN_UNUSED_RESULT static Handle<Derived> Add(
      LocalIsolate* isolate, Handle<Derived> dictionary, Key key,
      Handle<Object> value, PropertyDetails details,
      InternalIndex* entry_out = nullptr);

 protected:
  // Generic at put operation.
  V8_WARN_UNUSED_RESULT static Handle<Derived> AtPut(Isolate* isolate,
                                                     Handle<Derived> dictionary,
                                                     Key key,
                                                     Handle<Object> value,
                                                     PropertyDetails details);

  OBJECT_CONSTRUCTORS(Dictionary, HashTable<Derived, Shape>);
};

#define EXTERN_DECLARE_DICTIONARY(DERIVED, SHAPE)                  \
  EXTERN_DECLARE_HASH_TABLE(DERIVED, SHAPE)                        \
  extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) \
      Dictionary<DERIVED, SHAPE>;

template <typename Key>
class BaseDictionaryShape : public BaseShape<Key> {
 public:
  static const bool kHasDetails = true;
  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary dict, InternalIndex entry);

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary dict, InternalIndex entry,
                                  PropertyDetails value);
};

class NameDictionaryShape : public BaseDictionaryShape<Handle<Name>> {
 public:
  static inline bool IsMatch(Handle<Name> key, Object other);
  static inline uint32_t Hash(ReadOnlyRoots roots, Handle<Name> key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots, Object object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Name> key);
  static inline Handle<Object> AsHandle(OffThreadIsolate* isolate,
                                        Handle<Name> key);
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;
  static const int kEntryValueIndex = 1;
  static const bool kNeedsHoleCheck = false;
};

template <typename Derived, typename Shape>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) BaseNameDictionary
    : public Dictionary<Derived, Shape> {
  using Key = typename Shape::Key;

 public:
  static const int kNextEnumerationIndexIndex =
      HashTableBase::kPrefixStartIndex;
  static const int kObjectHashIndex = kNextEnumerationIndexIndex + 1;
  static const int kEntryValueIndex = 1;

  inline void SetHash(int hash);
  inline int Hash() const;

  // Creates a new dictionary.
  template <typename LocalIsolate>
  V8_WARN_UNUSED_RESULT static Handle<Derived> New(
      LocalIsolate* isolate, int at_least_space_for,
      AllocationType allocation = AllocationType::kYoung,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  // Collect the keys into the given KeyAccumulator, in ascending chronological
  // order of property creation.
  V8_WARN_UNUSED_RESULT static ExceptionStatus CollectKeysTo(
      Handle<Derived> dictionary, KeyAccumulator* keys);

  // Allocate the next enumeration index. Possibly updates all enumeration
  // indices in the table.
  static int NextEnumerationIndex(Isolate* isolate, Handle<Derived> dictionary);
  // Accessors for next enumeration index.
  inline int next_enumeration_index();
  inline void set_next_enumeration_index(int index);

  // Return the key indices sorted by its enumeration index.
  static Handle<FixedArray> IterationIndices(Isolate* isolate,
                                             Handle<Derived> dictionary);

  // Copies enumerable keys to preallocated fixed array.
  // Does not throw for uninitialized exports in module namespace objects, so
  // this has to be checked separately.
  static void CopyEnumKeysTo(Isolate* isolate, Handle<Derived> dictionary,
                             Handle<FixedArray> storage, KeyCollectionMode mode,
                             KeyAccumulator* accumulator);

  template <typename LocalIsolate>
  V8_WARN_UNUSED_RESULT static Handle<Derived> AddNoUpdateNextEnumerationIndex(
      LocalIsolate* isolate, Handle<Derived> dictionary, Key key,
      Handle<Object> value, PropertyDetails details,
      InternalIndex* entry_out = nullptr);

  V8_WARN_UNUSED_RESULT static Handle<Derived> Add(
      Isolate* isolate, Handle<Derived> dictionary, Key key,
      Handle<Object> value, PropertyDetails details,
      InternalIndex* entry_out = nullptr);

  OBJECT_CONSTRUCTORS(BaseNameDictionary, Dictionary<Derived, Shape>);
};

#define EXTERN_DECLARE_BASE_NAME_DICTIONARY(DERIVED, SHAPE)        \
  EXTERN_DECLARE_DICTIONARY(DERIVED, SHAPE)                        \
  extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) \
      BaseNameDictionary<DERIVED, SHAPE>;

EXTERN_DECLARE_BASE_NAME_DICTIONARY(NameDictionary, NameDictionaryShape)

class V8_EXPORT_PRIVATE NameDictionary
    : public BaseNameDictionary<NameDictionary, NameDictionaryShape> {
 public:
  DECL_CAST(NameDictionary)

  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;
  static const int kInitialCapacity = 2;

  inline Name NameAt(InternalIndex entry);
  inline Name NameAt(const Isolate* isolate, InternalIndex entry);

  inline void set_hash(int hash);
  inline int hash() const;

  OBJECT_CONSTRUCTORS(NameDictionary,
                      BaseNameDictionary<NameDictionary, NameDictionaryShape>);
};

class V8_EXPORT_PRIVATE GlobalDictionaryShape : public NameDictionaryShape {
 public:
  static inline bool IsMatch(Handle<Name> key, Object other);
  static inline uint32_t HashForObject(ReadOnlyRoots roots, Object object);

  static const int kEntrySize = 1;  // Overrides NameDictionaryShape::kEntrySize

  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary dict, InternalIndex entry);

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary dict, InternalIndex entry,
                                  PropertyDetails value);

  static inline Object Unwrap(Object key);
  static inline bool IsKey(ReadOnlyRoots roots, Object k);
  static inline bool IsLive(ReadOnlyRoots roots, Object key);
  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
};

EXTERN_DECLARE_BASE_NAME_DICTIONARY(GlobalDictionary, GlobalDictionaryShape)

class V8_EXPORT_PRIVATE GlobalDictionary
    : public BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape> {
 public:
  DECL_CAST(GlobalDictionary)

  inline Object ValueAt(InternalIndex entry);
  inline Object ValueAt(const Isolate* isolate, InternalIndex entry);
  inline PropertyCell CellAt(InternalIndex entry);
  inline PropertyCell CellAt(const Isolate* isolate, InternalIndex entry);
  inline void SetEntry(InternalIndex entry, Object key, Object value,
                       PropertyDetails details);
  inline Name NameAt(InternalIndex entry);
  inline Name NameAt(const Isolate* isolate, InternalIndex entry);
  inline void ValueAtPut(InternalIndex entry, Object value);

  OBJECT_CONSTRUCTORS(
      GlobalDictionary,
      BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>);
};

class NumberDictionaryBaseShape : public BaseDictionaryShape<uint32_t> {
 public:
  static inline bool IsMatch(uint32_t key, Object other);
  static inline Handle<Object> AsHandle(Isolate* isolate, uint32_t key);
  static inline Handle<Object> AsHandle(OffThreadIsolate* isolate,
                                        uint32_t key);

  static inline uint32_t Hash(ReadOnlyRoots roots, uint32_t key);
  static inline uint32_t HashForObject(ReadOnlyRoots roots, Object object);
};

class NumberDictionaryShape : public NumberDictionaryBaseShape {
 public:
  static const int kPrefixSize = 1;
  static const int kEntrySize = 3;

  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
};

class SimpleNumberDictionaryShape : public NumberDictionaryBaseShape {
 public:
  static const bool kHasDetails = false;
  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;

  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary dict,
                                          InternalIndex entry) {
    UNREACHABLE();
  }

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary dict, InternalIndex entry,
                                  PropertyDetails value) {
    UNREACHABLE();
  }

  static inline Handle<Map> GetMap(ReadOnlyRoots roots);
};

EXTERN_DECLARE_DICTIONARY(SimpleNumberDictionary, SimpleNumberDictionaryShape)

// SimpleNumberDictionary is used to map number to an entry.
class SimpleNumberDictionary
    : public Dictionary<SimpleNumberDictionary, SimpleNumberDictionaryShape> {
 public:
  DECL_CAST(SimpleNumberDictionary)
  // Type specific at put (default NONE attributes is used when adding).
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Handle<SimpleNumberDictionary>
  Set(Isolate* isolate, Handle<SimpleNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value);

  static const int kEntryValueIndex = 1;

  OBJECT_CONSTRUCTORS(
      SimpleNumberDictionary,
      Dictionary<SimpleNumberDictionary, SimpleNumberDictionaryShape>);
};

EXTERN_DECLARE_DICTIONARY(NumberDictionary, NumberDictionaryShape)

// NumberDictionary is used as elements backing store and provides a bitfield
// and stores property details for every entry.
class NumberDictionary
    : public Dictionary<NumberDictionary, NumberDictionaryShape> {
 public:
  DECL_CAST(NumberDictionary)
  DECL_PRINTER(NumberDictionary)

  // Type specific at put (default NONE attributes is used when adding).
  V8_WARN_UNUSED_RESULT static Handle<NumberDictionary> Set(
      Isolate* isolate, Handle<NumberDictionary> dictionary, uint32_t key,
      Handle<Object> value,
      Handle<JSObject> dictionary_holder = Handle<JSObject>::null(),
      PropertyDetails details = PropertyDetails::Empty());

  static const int kMaxNumberKeyIndex = kPrefixStartIndex;
  void UpdateMaxNumberKey(uint32_t key, Handle<JSObject> dictionary_holder);

  // Sorting support
  void CopyValuesTo(FixedArray elements);

  // If slow elements are required we will never go back to fast-case
  // for the elements kept in this dictionary.  We require slow
  // elements if an element has been added at an index larger than
  // kRequiresSlowElementsLimit or set_requires_slow_elements() has been called
  // when defining a getter or setter with a number key.
  inline bool requires_slow_elements();
  inline void set_requires_slow_elements();

  // Get the value of the max number key that has been added to this
  // dictionary.  max_number_key can only be called if
  // requires_slow_elements returns false.
  inline uint32_t max_number_key();

  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;

  // Bit masks.
  static const int kRequiresSlowElementsMask = 1;
  static const int kRequiresSlowElementsTagSize = 1;
  static const uint32_t kRequiresSlowElementsLimit = (1 << 29) - 1;

  // JSObjects prefer dictionary elements if the dictionary saves this much
  // memory compared to a fast elements backing store.
  static const uint32_t kPreferFastElementsSizeFactor = 3;

  OBJECT_CONSTRUCTORS(NumberDictionary,
                      Dictionary<NumberDictionary, NumberDictionaryShape>);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DICTIONARY_H_
