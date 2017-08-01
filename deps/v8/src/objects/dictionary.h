// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DICTIONARY_H_
#define V8_OBJECTS_DICTIONARY_H_

#include "src/objects/hash-table.h"

#include "src/base/export-template.h"
#include "src/globals.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class Isolate;

template <typename Derived, typename Shape, typename Key>
class Dictionary : public HashTable<Derived, Shape, Key> {
  typedef HashTable<Derived, Shape, Key> DerivedHashTable;

 public:
  // Returns the value at entry.
  Object* ValueAt(int entry) {
    return this->get(Derived::EntryToIndex(entry) + 1);
  }

  // Set the value for entry.
  void ValueAtPut(int entry, Object* value) {
    this->set(Derived::EntryToIndex(entry) + 1, value);
  }

  // Returns the property details for the property at entry.
  PropertyDetails DetailsAt(int entry) {
    return Shape::DetailsAt(static_cast<Derived*>(this), entry);
  }

  // Set the details for entry.
  void DetailsAtPut(int entry, PropertyDetails value) {
    Shape::DetailsAtPut(static_cast<Derived*>(this), entry, value);
  }

  // Returns true if property at given entry is deleted.
  bool IsDeleted(int entry) {
    return Shape::IsDeleted(static_cast<Derived*>(this), entry);
  }

  // Delete a property from the dictionary.
  static Handle<Object> DeleteProperty(Handle<Derived> dictionary, int entry);

  // Attempt to shrink the dictionary after deletion of key.
  MUST_USE_RESULT static inline Handle<Derived> Shrink(
      Handle<Derived> dictionary, Key key) {
    return DerivedHashTable::Shrink(dictionary, key);
  }

  // Returns the number of elements in the dictionary filtering out properties
  // with the specified attributes.
  int NumberOfElementsFilterAttributes(PropertyFilter filter);

  // Returns the number of enumerable elements in the dictionary.
  int NumberOfEnumElements() {
    return NumberOfElementsFilterAttributes(ENUMERABLE_STRINGS);
  }

  enum SortMode { UNSORTED, SORTED };

  // Return the key indices sorted by its enumeration index.
  static Handle<FixedArray> IterationIndices(
      Handle<Dictionary<Derived, Shape, Key>> dictionary);

  // Collect the keys into the given KeyAccumulator, in ascending chronological
  // order of property creation.
  static void CollectKeysTo(Handle<Dictionary<Derived, Shape, Key>> dictionary,
                            KeyAccumulator* keys);

  // Copies enumerable keys to preallocated fixed array.
  static void CopyEnumKeysTo(Handle<Dictionary<Derived, Shape, Key>> dictionary,
                             Handle<FixedArray> storage, KeyCollectionMode mode,
                             KeyAccumulator* accumulator);

  // Accessors for next enumeration index.
  void SetNextEnumerationIndex(int index) {
    DCHECK(index != 0);
    this->set(kNextEnumerationIndexIndex, Smi::FromInt(index));
  }

  int NextEnumerationIndex() {
    return Smi::cast(this->get(kNextEnumerationIndexIndex))->value();
  }

  // Creates a new dictionary.
  MUST_USE_RESULT static Handle<Derived> New(
      Isolate* isolate, int at_least_space_for,
      PretenureFlag pretenure = NOT_TENURED,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  // Creates an dictionary with minimal possible capacity.
  MUST_USE_RESULT static Handle<Derived> NewEmpty(
      Isolate* isolate, PretenureFlag pretenure = NOT_TENURED);

  // Ensures that a new dictionary is created when the capacity is checked.
  void SetRequiresCopyOnCapacityChange();

  // Ensure enough space for n additional elements.
  static Handle<Derived> EnsureCapacity(Handle<Derived> obj, int n, Key key);

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  void Print(std::ostream& os);  // NOLINT
#endif
  // Returns the key (slow).
  Object* SlowReverseLookup(Object* value);

  // Sets the entry to (key, value) pair.
  inline void SetEntry(int entry, Handle<Object> key, Handle<Object> value);
  inline void SetEntry(int entry, Handle<Object> key, Handle<Object> value,
                       PropertyDetails details);

  MUST_USE_RESULT static Handle<Derived> Add(Handle<Derived> dictionary,
                                             Key key, Handle<Object> value,
                                             PropertyDetails details,
                                             int* entry_out = nullptr);

  static const int kMaxNumberKeyIndex = DerivedHashTable::kPrefixStartIndex;
  static const int kNextEnumerationIndexIndex = kMaxNumberKeyIndex + 1;

  static const bool kIsEnumerable = Shape::kIsEnumerable;

 protected:
  // Generic at put operation.
  MUST_USE_RESULT static Handle<Derived> AtPut(Handle<Derived> dictionary,
                                               Key key, Handle<Object> value);
  // Add entry to dictionary. Returns entry value.
  static int AddEntry(Handle<Derived> dictionary, Key key, Handle<Object> value,
                      PropertyDetails details, uint32_t hash);
};

template <typename Derived, typename Shape>
class NameDictionaryBase : public Dictionary<Derived, Shape, Handle<Name>> {
  typedef Dictionary<Derived, Shape, Handle<Name>> DerivedDictionary;

 public:
  // Find entry for key, otherwise return kNotFound. Optimized version of
  // HashTable::FindEntry.
  int FindEntry(Handle<Name> key);
};

template <typename Key>
class BaseDictionaryShape : public BaseShape<Key> {
 public:
  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry) {
    STATIC_ASSERT(Dictionary::kEntrySize == 3);
    DCHECK(entry >= 0);  // Not found is -1, which is not caught by get().
    return PropertyDetails(Smi::cast(dict->get(
        Dictionary::EntryToIndex(entry) + Dictionary::kEntryDetailsIndex)));
  }

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary* dict, int entry,
                                  PropertyDetails value) {
    STATIC_ASSERT(Dictionary::kEntrySize == 3);
    dict->set(Dictionary::EntryToIndex(entry) + Dictionary::kEntryDetailsIndex,
              value.AsSmi());
  }

  template <typename Dictionary>
  static bool IsDeleted(Dictionary* dict, int entry) {
    return false;
  }

  template <typename Dictionary>
  static inline void SetEntry(Dictionary* dict, int entry, Handle<Object> key,
                              Handle<Object> value, PropertyDetails details);
};

class NameDictionaryShape : public BaseDictionaryShape<Handle<Name>> {
 public:
  static inline bool IsMatch(Handle<Name> key, Object* other);
  static inline uint32_t Hash(Handle<Name> key);
  static inline uint32_t HashForObject(Handle<Name> key, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Name> key);
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;
  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;
  static const bool kIsEnumerable = true;
};

class NameDictionary
    : public NameDictionaryBase<NameDictionary, NameDictionaryShape> {
  typedef NameDictionaryBase<NameDictionary, NameDictionaryShape>
      DerivedDictionary;

 public:
  DECLARE_CAST(NameDictionary)

  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;
  static const int kInitialCapacity = 2;
};

class GlobalDictionaryShape : public NameDictionaryShape {
 public:
  static const int kEntrySize = 2;  // Overrides NameDictionaryShape::kEntrySize

  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry);

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary* dict, int entry,
                                  PropertyDetails value);

  template <typename Dictionary>
  static bool IsDeleted(Dictionary* dict, int entry);

  template <typename Dictionary>
  static inline void SetEntry(Dictionary* dict, int entry, Handle<Object> key,
                              Handle<Object> value, PropertyDetails details);
};

class GlobalDictionary
    : public NameDictionaryBase<GlobalDictionary, GlobalDictionaryShape> {
 public:
  DECLARE_CAST(GlobalDictionary)

  static const int kEntryValueIndex = 1;
};

class NumberDictionaryShape : public BaseDictionaryShape<uint32_t> {
 public:
  static inline bool IsMatch(uint32_t key, Object* other);
  static inline Handle<Object> AsHandle(Isolate* isolate, uint32_t key);
  static const bool kIsEnumerable = false;
};

class SeededNumberDictionaryShape : public NumberDictionaryShape {
 public:
  static const bool UsesSeed = true;
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;

  static inline uint32_t SeededHash(uint32_t key, uint32_t seed);
  static inline uint32_t SeededHashForObject(uint32_t key, uint32_t seed,
                                             Object* object);
};

class UnseededNumberDictionaryShape : public NumberDictionaryShape {
 public:
  static const int kPrefixSize = 0;
  static const int kEntrySize = 2;

  static inline uint32_t Hash(uint32_t key);
  static inline uint32_t HashForObject(uint32_t key, Object* object);

  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry) {
    UNREACHABLE();
    return PropertyDetails::Empty();
  }

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary* dict, int entry,
                                  PropertyDetails value) {
    UNREACHABLE();
  }

  static inline Map* GetMap(Isolate* isolate);
};

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    HashTable<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>;

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>;

class SeededNumberDictionary
    : public Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape,
                        uint32_t> {
 public:
  DECLARE_CAST(SeededNumberDictionary)

  // Type specific at put (default NONE attributes is used when adding).
  MUST_USE_RESULT static Handle<SeededNumberDictionary> AtNumberPut(
      Handle<SeededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value, Handle<JSObject> dictionary_holder);
  MUST_USE_RESULT static Handle<SeededNumberDictionary> AddNumberEntry(
      Handle<SeededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value, PropertyDetails details,
      Handle<JSObject> dictionary_holder);

  // Set an existing entry or add a new one if needed.
  // Return the updated dictionary.
  MUST_USE_RESULT static Handle<SeededNumberDictionary> Set(
      Handle<SeededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value, PropertyDetails details,
      Handle<JSObject> dictionary_holder);

  void UpdateMaxNumberKey(uint32_t key, Handle<JSObject> dictionary_holder);

  // Returns true if the dictionary contains any elements that are non-writable,
  // non-configurable, non-enumerable, or have getters/setters.
  bool HasComplexElements();

  // Sorting support
  void CopyValuesTo(FixedArray* elements);

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
};

class UnseededNumberDictionary
    : public Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape,
                        uint32_t> {
 public:
  DECLARE_CAST(UnseededNumberDictionary)

  // Type specific at put (default NONE attributes is used when adding).
  MUST_USE_RESULT static Handle<UnseededNumberDictionary> AtNumberPut(
      Handle<UnseededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value);
  MUST_USE_RESULT static Handle<UnseededNumberDictionary> AddNumberEntry(
      Handle<UnseededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value);
  static Handle<UnseededNumberDictionary> DeleteKey(
      Handle<UnseededNumberDictionary> dictionary, uint32_t key);

  // Set an existing entry or add a new one if needed.
  // Return the updated dictionary.
  MUST_USE_RESULT static Handle<UnseededNumberDictionary> Set(
      Handle<UnseededNumberDictionary> dictionary, uint32_t key,
      Handle<Object> value);

  static const int kEntryValueIndex = 1;
  static const int kEntryDetailsIndex = 2;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DICTIONARY_H_
