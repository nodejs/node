// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DESCRIPTOR_ARRAY_H_
#define V8_OBJECTS_DESCRIPTOR_ARRAY_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class Isolate;

// A DescriptorArray is a fixed array used to hold instance descriptors.
// The format of the these objects is:
//   [0]: Number of descriptors
//   [1]: Either Smi(0) if uninitialized,
//        or enum cache bridge (FixedArray[2]):
//          [0]: enum cache: FixedArray containing all own enumerable keys
//          [1]: either Smi(0) or pointer to fixed array with indices
//   [2]: first key (and internalized String)
//   [3]: first descriptor details (see PropertyDetails)
//   [4]: first value for constants | Smi(1) when not usedA
//
//   [2 + number of descriptors * 3]: start of slack
class DescriptorArray : public FixedArray {
 public:
  // Returns true for both shared empty_descriptor_array and for smis, which the
  // map uses to encode additional bit fields when the descriptor array is not
  // yet used.
  inline bool IsEmpty();

  // Returns the number of descriptors in the array.
  inline int number_of_descriptors();
  inline int number_of_descriptors_storage();
  inline int NumberOfSlackDescriptors();

  inline void SetNumberOfDescriptors(int number_of_descriptors);
  inline int number_of_entries();

  inline bool HasEnumCache();
  inline bool HasEnumIndicesCache();
  inline FixedArray* GetEnumCache();
  inline FixedArray* GetEnumIndicesCache();

  void ClearEnumCache();
  inline void CopyEnumCacheFrom(DescriptorArray* array);
  // Initialize or change the enum cache,
  // using the supplied storage for the small "bridge".
  static void SetEnumCache(Handle<DescriptorArray> descriptors,
                           Isolate* isolate, Handle<FixedArray> new_cache,
                           Handle<FixedArray> new_index_cache);

  // Accessors for fetching instance descriptor at descriptor number.
  inline Name* GetKey(int descriptor_number);
  inline Object** GetKeySlot(int descriptor_number);
  inline Object* GetValue(int descriptor_number);
  inline void SetValue(int descriptor_number, Object* value);
  inline Object** GetValueSlot(int descriptor_number);
  static inline int GetValueOffset(int descriptor_number);
  inline Object** GetDescriptorStartSlot(int descriptor_number);
  inline Object** GetDescriptorEndSlot(int descriptor_number);
  inline PropertyDetails GetDetails(int descriptor_number);
  inline int GetFieldIndex(int descriptor_number);
  inline FieldType* GetFieldType(int descriptor_number);

  inline Name* GetSortedKey(int descriptor_number);
  inline int GetSortedKeyIndex(int descriptor_number);
  inline void SetSortedKey(int pointer, int descriptor_number);

  // Accessor for complete descriptor.
  inline void Get(int descriptor_number, Descriptor* desc);
  inline void Set(int descriptor_number, Descriptor* desc);
  inline void Set(int descriptor_number, Name* key, Object* value,
                  PropertyDetails details);
  void Replace(int descriptor_number, Descriptor* descriptor);

  // Generalizes constness, representation and field type of all field
  // descriptors.
  void GeneralizeAllFields();

  // Append automatically sets the enumeration index. This should only be used
  // to add descriptors in bulk at the end, followed by sorting the descriptor
  // array.
  inline void Append(Descriptor* desc);

  static Handle<DescriptorArray> CopyUpTo(Handle<DescriptorArray> desc,
                                          int enumeration_index, int slack = 0);

  static Handle<DescriptorArray> CopyUpToAddAttributes(
      Handle<DescriptorArray> desc, int enumeration_index,
      PropertyAttributes attributes, int slack = 0);

  // Sort the instance descriptors by the hash codes of their keys.
  void Sort();

  // Search the instance descriptors for given name.
  INLINE(int Search(Name* name, int number_of_own_descriptors));

  // As the above, but uses DescriptorLookupCache and updates it when
  // necessary.
  INLINE(int SearchWithCache(Isolate* isolate, Name* name, Map* map));

  bool IsEqualUpTo(DescriptorArray* desc, int nof_descriptors);

  // Allocates a DescriptorArray, but returns the singleton
  // empty descriptor array object if number_of_descriptors is 0.
  static Handle<DescriptorArray> Allocate(
      Isolate* isolate, int number_of_descriptors, int slack,
      PretenureFlag pretenure = NOT_TENURED);

  DECLARE_CAST(DescriptorArray)

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  static const int kDescriptorLengthIndex = 0;
  static const int kEnumCacheBridgeIndex = 1;
  static const int kFirstIndex = 2;

  // The length of the "bridge" to the enum cache.
  static const int kEnumCacheBridgeLength = 2;
  static const int kEnumCacheBridgeCacheIndex = 0;
  static const int kEnumCacheBridgeIndicesCacheIndex = 1;

  // Layout description.
  static const int kDescriptorLengthOffset = FixedArray::kHeaderSize;
  static const int kEnumCacheBridgeOffset =
      kDescriptorLengthOffset + kPointerSize;
  static const int kFirstOffset = kEnumCacheBridgeOffset + kPointerSize;

  // Layout description for the bridge array.
  static const int kEnumCacheBridgeCacheOffset = FixedArray::kHeaderSize;

  // Layout of descriptor.
  // Naming is consistent with Dictionary classes for easy templating.
  static const int kEntryKeyIndex = 0;
  static const int kEntryDetailsIndex = 1;
  static const int kEntryValueIndex = 2;
  static const int kEntrySize = 3;

#if defined(DEBUG) || defined(OBJECT_PRINT)
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  // Print all the descriptors.
  void PrintDescriptors(std::ostream& os);  // NOLINT

  void PrintDescriptorDetails(std::ostream& os, int descriptor,
                              PropertyDetails::PrintMode mode);
#endif

#ifdef DEBUG
  // Is the descriptor array sorted and without duplicates?
  bool IsSortedNoDuplicates(int valid_descriptors = -1);

  // Is the descriptor array consistent with the back pointers in targets?
  bool IsConsistentWithBackPointers(Map* current_map);

  // Are two DescriptorArrays equal?
  bool IsEqualTo(DescriptorArray* other);
#endif

  // Returns the fixed array length required to hold number_of_descriptors
  // descriptors.
  static int LengthFor(int number_of_descriptors) {
    return ToKeyIndex(number_of_descriptors);
  }

  static int ToDetailsIndex(int descriptor_number) {
    return kFirstIndex + (descriptor_number * kEntrySize) + kEntryDetailsIndex;
  }

  // Conversion from descriptor number to array indices.
  static int ToKeyIndex(int descriptor_number) {
    return kFirstIndex + (descriptor_number * kEntrySize) + kEntryKeyIndex;
  }

  static int ToValueIndex(int descriptor_number) {
    return kFirstIndex + (descriptor_number * kEntrySize) + kEntryValueIndex;
  }

 private:
  // Transfer a complete descriptor from the src descriptor array to this
  // descriptor array.
  void CopyFrom(int index, DescriptorArray* src);

  // Swap first and second descriptor.
  inline void SwapSortedKeys(int first, int second);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DescriptorArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DESCRIPTOR_ARRAY_H_
