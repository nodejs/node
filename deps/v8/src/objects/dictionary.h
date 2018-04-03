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

template <typename Derived, typename Shape>
class Dictionary : public HashTable<Derived, Shape> {
  typedef HashTable<Derived, Shape> DerivedHashTable;

 public:
  typedef typename Shape::Key Key;
  // Returns the value at entry.
  Object* ValueAt(int entry) {
    return this->get(DerivedHashTable::EntryToIndex(entry) + 1);
  }

  // Set the value for entry.
  void ValueAtPut(int entry, Object* value) {
    this->set(DerivedHashTable::EntryToIndex(entry) + 1, value);
  }

  // Returns the property details for the property at entry.
  PropertyDetails DetailsAt(int entry) {
    return Shape::DetailsAt(static_cast<Derived*>(this), entry);
  }

  // Set the details for entry.
  void DetailsAtPut(int entry, PropertyDetails value) {
    Shape::DetailsAtPut(static_cast<Derived*>(this), entry, value);
  }

  // Delete a property from the dictionary.
  MUST_USE_RESULT static Handle<Derived> DeleteEntry(Handle<Derived> dictionary,
                                                     int entry);

  // Attempt to shrink the dictionary after deletion of key.
  MUST_USE_RESULT static inline Handle<Derived> Shrink(
      Handle<Derived> dictionary) {
    return DerivedHashTable::Shrink(dictionary);
  }

  int NumberOfEnumerableProperties();

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  void Print(std::ostream& os);  // NOLINT
#endif
  // Returns the key (slow).
  Object* SlowReverseLookup(Object* value);

  // Sets the entry to (key, value) pair.
  inline void ClearEntry(int entry);
  inline void SetEntry(int entry, Object* key, Object* value,
                       PropertyDetails details);

  MUST_USE_RESULT static Handle<Derived> Add(Handle<Derived> dictionary,
                                             Key key, Handle<Object> value,
                                             PropertyDetails details,
                                             int* entry_out = nullptr);

 protected:
  // Generic at put operation.
  MUST_USE_RESULT static Handle<Derived> AtPut(Handle<Derived> dictionary,
                                               Key key, Handle<Object> value,
                                               PropertyDetails details);
};

template <typename Key>
class BaseDictionaryShape : public BaseShape<Key> {
 public:
  static const bool kHasDetails = true;
  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry) {
    STATIC_ASSERT(Dictionary::kEntrySize == 3);
    DCHECK_GE(entry, 0);  // Not found is -1, which is not caught by get().
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
};

class NameDictionaryShape : public BaseDictionaryShape<Handle<Name>> {
 public:
  static inline bool IsMatch(Handle<Name> key, Object* other);
  static inline uint32_t Hash(Isolate* isolate, Handle<Name> key);
  static inline uint32_t HashForObject(Isolate* isolate, Object* object);
  static inline Handle<Object> AsHandle(Isolate* isolate, Handle<Name> key);
  static inline int GetMapRootIndex();
  static const int kPrefixSize = 2;
  static const int kEntrySize = 3;
  static const int kEntryValueIndex = 1;
  static const bool kNeedsHoleCheck = false;
};

template <typename Derived, typename Shape>
class BaseNameDictionary : public Dictionary<Derived, Shape> {
  typedef typename Shape::Key Key;

 public:
  static const int kNextEnumerationIndexIndex =
      HashTableBase::kPrefixStartIndex;
  static const int kObjectHashIndex = kNextEnumerationIndexIndex + 1;
  static const int kEntryValueIndex = 1;

  // Accessors for next enumeration index.
  void SetNextEnumerationIndex(int index) {
    DCHECK_NE(0, index);
    this->set(kNextEnumerationIndexIndex, Smi::FromInt(index));
  }

  int NextEnumerationIndex() {
    return Smi::ToInt(this->get(kNextEnumerationIndexIndex));
  }

  void SetHash(int hash) {
    DCHECK(PropertyArray::HashField::is_valid(hash));
    this->set(kObjectHashIndex, Smi::FromInt(hash));
  }

  int Hash() const {
    Object* hash_obj = this->get(kObjectHashIndex);
    int hash = Smi::ToInt(hash_obj);
    DCHECK(PropertyArray::HashField::is_valid(hash));
    return hash;
  }

  // Creates a new dictionary.
  MUST_USE_RESULT static Handle<Derived> New(
      Isolate* isolate, int at_least_space_for,
      PretenureFlag pretenure = NOT_TENURED,
      MinimumCapacity capacity_option = USE_DEFAULT_MINIMUM_CAPACITY);

  // Collect the keys into the given KeyAccumulator, in ascending chronological
  // order of property creation.
  static void CollectKeysTo(Handle<Derived> dictionary, KeyAccumulator* keys);

  // Return the key indices sorted by its enumeration index.
  static Handle<FixedArray> IterationIndices(Handle<Derived> dictionary);

  // Copies enumerable keys to preallocated fixed array.
  static void CopyEnumKeysTo(Handle<Derived> dictionary,
                             Handle<FixedArray> storage, KeyCollectionMode mode,
                             KeyAccumulator* accumulator);

  // Ensure enough space for n additional elements.
  static Handle<Derived> EnsureCapacity(Handle<Derived> dictionary, int n);

  MUST_USE_RESULT static Handle<Derived> AddNoUpdateNextEnumerationIndex(
      Handle<Derived> dictionary, Key key, Handle<Object> value,
      PropertyDetails details, int* entry_out = nullptr);

  MUST_USE_RESULT static Handle<Derived> Add(Handle<Derived> dictionary,
                                             Key key, Handle<Object> value,
                                             PropertyDetails details,
                                             int* entry_out = nullptr);
};

class NameDictionary
    : public BaseNameDictionary<NameDictionary, NameDictionaryShape> {
 public:
  DECL_CAST(NameDictionary)

  static const int kEntryDetailsIndex = 2;
  static const int kInitialCapacity = 2;

  inline Name* NameAt(int entry);
  inline void set_hash(int hash);
  inline int hash() const;
};

class GlobalDictionaryShape : public NameDictionaryShape {
 public:
  static inline bool IsMatch(Handle<Name> key, Object* other);
  static inline uint32_t HashForObject(Isolate* isolate, Object* object);

  static const int kEntrySize = 1;  // Overrides NameDictionaryShape::kEntrySize

  template <typename Dictionary>
  static inline PropertyDetails DetailsAt(Dictionary* dict, int entry);

  template <typename Dictionary>
  static inline void DetailsAtPut(Dictionary* dict, int entry,
                                  PropertyDetails value);

  static inline Object* Unwrap(Object* key);
  static inline bool IsKey(Isolate* isolate, Object* k);
  static inline bool IsLive(Isolate* isolate, Object* key);
  static inline int GetMapRootIndex();
};

class GlobalDictionary
    : public BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape> {
 public:
  DECL_CAST(GlobalDictionary)

  inline Object* ValueAt(int entry);
  inline PropertyCell* CellAt(int entry);
  inline void SetEntry(int entry, Object* key, Object* value,
                       PropertyDetails details);
  inline Name* NameAt(int entry);
  inline void ValueAtPut(int entry, Object* value);
};

class NumberDictionaryShape : public BaseDictionaryShape<uint32_t> {
 public:
  static const int kPrefixSize = 1;
  static const int kEntrySize = 3;

  static inline bool IsMatch(uint32_t key, Object* other);
  static inline Handle<Object> AsHandle(Isolate* isolate, uint32_t key);

  static inline uint32_t Hash(Isolate* isolate, uint32_t key);
  static inline uint32_t HashForObject(Isolate* isolate, Object* object);

  static inline int GetMapRootIndex();
};

extern template class EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) HashTable<NumberDictionary, NumberDictionaryShape>;

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Dictionary<NumberDictionary, NumberDictionaryShape>;

class NumberDictionary
    : public Dictionary<NumberDictionary, NumberDictionaryShape> {
 public:
  DECL_CAST(NumberDictionary)

  // Type specific at put (default NONE attributes is used when adding).
  MUST_USE_RESULT static Handle<NumberDictionary> Set(
      Handle<NumberDictionary> dictionary, uint32_t key, Handle<Object> value,
      Handle<JSObject> dictionary_holder = Handle<JSObject>::null(),
      PropertyDetails details = PropertyDetails::Empty());

  static const int kMaxNumberKeyIndex = kPrefixStartIndex;
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DICTIONARY_H_
