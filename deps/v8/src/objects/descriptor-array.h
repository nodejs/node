// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DESCRIPTOR_ARRAY_H_
#define V8_OBJECTS_DESCRIPTOR_ARRAY_H_

#include "src/objects.h"
#include "src/objects/fixed-array.h"
#include "src/objects/struct.h"
#include "src/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class Isolate;

// An EnumCache is a pair used to hold keys and indices caches.
class EnumCache : public Tuple2 {
 public:
  DECL_ACCESSORS(keys, FixedArray)
  DECL_ACCESSORS(indices, FixedArray)

  DECL_CAST(EnumCache)

  // Layout description.
  static const int kKeysOffset = kValue1Offset;
  static const int kIndicesOffset = kValue2Offset;

  OBJECT_CONSTRUCTORS(EnumCache, Tuple2);
};

// A DescriptorArray is a custom array that holds instance descriptors.
// It has the following layout:
//   Header:
//     [16:0  bits]: number_of_all_descriptors (including slack)
//     [32:16 bits]: number_of_descriptors
//     [48:32 bits]: raw_number_of_marked_descriptors (used by GC)
//     [64:48 bits]: alignment filler
//     [kEnumCacheOffset]: enum cache
//   Elements:
//     [kHeaderSize + 0]: first key (and internalized String)
//     [kHeaderSize + 1]: first descriptor details (see PropertyDetails)
//     [kHeaderSize + 2]: first value for constants / Smi(1) when not used
//   Slack:
//     [kHeaderSize + number of descriptors * 3]: start of slack
// The "value" fields store either values or field types. A field type is either
// FieldType::None(), FieldType::Any() or a weak reference to a Map. All other
// references are strong.
class DescriptorArray : public HeapObject {
 public:
  DECL_INT16_ACCESSORS(number_of_all_descriptors)
  DECL_INT16_ACCESSORS(number_of_descriptors)
  inline int16_t number_of_slack_descriptors() const;
  inline int number_of_entries() const;
  DECL_ACCESSORS(enum_cache, EnumCache)

  void ClearEnumCache();
  inline void CopyEnumCacheFrom(DescriptorArray array);
  static void InitializeOrChangeEnumCache(Handle<DescriptorArray> descriptors,
                                          Isolate* isolate,
                                          Handle<FixedArray> keys,
                                          Handle<FixedArray> indices);

  // Accessors for fetching instance descriptor at descriptor number.
  inline Name GetKey(int descriptor_number) const;
  inline Object GetStrongValue(int descriptor_number);
  inline void SetValue(int descriptor_number, Object value);
  inline MaybeObject GetValue(int descriptor_number);
  inline PropertyDetails GetDetails(int descriptor_number);
  inline int GetFieldIndex(int descriptor_number);
  inline FieldType GetFieldType(int descriptor_number);

  inline Name GetSortedKey(int descriptor_number);
  inline int GetSortedKeyIndex(int descriptor_number);
  inline void SetSortedKey(int pointer, int descriptor_number);

  // Accessor for complete descriptor.
  inline void Set(int descriptor_number, Descriptor* desc);
  inline void Set(int descriptor_number, Name key, MaybeObject value,
                  PropertyDetails details);
  void Replace(int descriptor_number, Descriptor* descriptor);

  // Generalizes constness, representation and field type of all field
  // descriptors.
  void GeneralizeAllFields();

  // Append automatically sets the enumeration index. This should only be used
  // to add descriptors in bulk at the end, followed by sorting the descriptor
  // array.
  inline void Append(Descriptor* desc);

  static Handle<DescriptorArray> CopyUpTo(Isolate* isolate,
                                          Handle<DescriptorArray> desc,
                                          int enumeration_index, int slack = 0);

  static Handle<DescriptorArray> CopyUpToAddAttributes(
      Isolate* isolate, Handle<DescriptorArray> desc, int enumeration_index,
      PropertyAttributes attributes, int slack = 0);

  static Handle<DescriptorArray> CopyForFastObjectClone(
      Isolate* isolate, Handle<DescriptorArray> desc, int enumeration_index,
      int slack = 0);

  // Sort the instance descriptors by the hash codes of their keys.
  void Sort();

  // Search the instance descriptors for given name.
  V8_INLINE int Search(Name name, int number_of_own_descriptors);
  V8_INLINE int Search(Name name, Map map);

  // As the above, but uses DescriptorLookupCache and updates it when
  // necessary.
  V8_INLINE int SearchWithCache(Isolate* isolate, Name name, Map map);

  bool IsEqualUpTo(DescriptorArray desc, int nof_descriptors);

  // Allocates a DescriptorArray, but returns the singleton
  // empty descriptor array object if number_of_descriptors is 0.
  static Handle<DescriptorArray> Allocate(
      Isolate* isolate, int nof_descriptors, int slack,
      PretenureFlag pretenure = NOT_TENURED);

  void Initialize(EnumCache enum_cache, HeapObject undefined_value,
                  int nof_descriptors, int slack);

  DECL_CAST(DescriptorArray)

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  // Layout description.
#define DESCRIPTOR_ARRAY_FIELDS(V)                    \
  V(kNumberOfAllDescriptorsOffset, kUInt16Size)       \
  V(kNumberOfDescriptorsOffset, kUInt16Size)          \
  V(kRawNumberOfMarkedDescriptorsOffset, kUInt16Size) \
  V(kFiller16BitsOffset, kUInt16Size)                 \
  V(kPointersStartOffset, 0)                          \
  V(kEnumCacheOffset, kTaggedSize)                    \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                DESCRIPTOR_ARRAY_FIELDS)
#undef DESCRIPTOR_ARRAY_FIELDS

  STATIC_ASSERT(IsAligned(kPointersStartOffset, kTaggedSize));
  STATIC_ASSERT(IsAligned(kHeaderSize, kTaggedSize));

  // Garbage collection support.
  DECL_INT16_ACCESSORS(raw_number_of_marked_descriptors)
  // Atomic compare-and-swap operation on the raw_number_of_marked_descriptors.
  int16_t CompareAndSwapRawNumberOfMarkedDescriptors(int16_t expected,
                                                     int16_t value);
  int16_t UpdateNumberOfMarkedDescriptors(unsigned mark_compact_epoch,
                                          int16_t number_of_marked_descriptors);

  static constexpr int SizeFor(int number_of_all_descriptors) {
    return offset(number_of_all_descriptors * kEntrySize);
  }
  static constexpr int OffsetOfDescriptorAt(int descriptor) {
    return offset(descriptor * kEntrySize);
  }
  inline ObjectSlot GetFirstPointerSlot();
  inline ObjectSlot GetDescriptorSlot(int descriptor);
  inline ObjectSlot GetKeySlot(int descriptor);
  inline MaybeObjectSlot GetValueSlot(int descriptor);

  typedef FlexibleWeakBodyDescriptor<kPointersStartOffset> BodyDescriptor;

  // Layout of descriptor.
  // Naming is consistent with Dictionary classes for easy templating.
  static const int kEntryKeyIndex = 0;
  static const int kEntryDetailsIndex = 1;
  static const int kEntryValueIndex = 2;
  static const int kEntrySize = 3;

  // Print all the descriptors.
  void PrintDescriptors(std::ostream& os);
  void PrintDescriptorDetails(std::ostream& os, int descriptor,
                              PropertyDetails::PrintMode mode);

  DECL_PRINTER(DescriptorArray)
  DECL_VERIFIER(DescriptorArray)

#ifdef DEBUG
  // Is the descriptor array sorted and without duplicates?
  bool IsSortedNoDuplicates(int valid_descriptors = -1);

  // Are two DescriptorArrays equal?
  bool IsEqualTo(DescriptorArray other);
#endif

  static constexpr int ToDetailsIndex(int descriptor_number) {
    return (descriptor_number * kEntrySize) + kEntryDetailsIndex;
  }

  // Conversion from descriptor number to array indices.
  static constexpr int ToKeyIndex(int descriptor_number) {
    return (descriptor_number * kEntrySize) + kEntryKeyIndex;
  }

  static constexpr int ToValueIndex(int descriptor_number) {
    return (descriptor_number * kEntrySize) + kEntryValueIndex;
  }

 private:
  DECL_INT16_ACCESSORS(filler16bits)
  // Low-level per-element accessors.
  static constexpr int offset(int index) {
    return kHeaderSize + index * kTaggedSize;
  }
  inline int length() const;
  inline MaybeObject get(int index) const;
  inline void set(int index, MaybeObject value);

  // Transfer a complete descriptor from the src descriptor array to this
  // descriptor array.
  void CopyFrom(int index, DescriptorArray src);

  // Swap first and second descriptor.
  inline void SwapSortedKeys(int first, int second);

  OBJECT_CONSTRUCTORS(DescriptorArray, HeapObject);
};

class NumberOfMarkedDescriptors {
 public:
// Bit positions for |bit_field|.
#define BIT_FIELD_FIELDS(V, _) \
  V(Epoch, unsigned, 2, _)     \
  V(Marked, int16_t, 14, _)
  DEFINE_BIT_FIELDS(BIT_FIELD_FIELDS)
#undef BIT_FIELD_FIELDS
  static const int kMaxNumberOfMarkedDescriptors = Marked::kMax;
  // Decodes the raw value of the number of marked descriptors for the
  // given mark compact garbage collection epoch.
  static inline int16_t decode(unsigned mark_compact_epoch, int16_t raw_value) {
    unsigned epoch_from_value = Epoch::decode(static_cast<uint16_t>(raw_value));
    int16_t marked_from_value =
        Marked::decode(static_cast<uint16_t>(raw_value));
    unsigned actual_epoch = mark_compact_epoch & Epoch::kMask;
    if (actual_epoch == epoch_from_value) return marked_from_value;
    // If the epochs do not match, then either the raw_value is zero (freshly
    // allocated descriptor array) or the epoch from value lags by 1.
    DCHECK_IMPLIES(raw_value != 0,
                   Epoch::decode(epoch_from_value + 1) == actual_epoch);
    // Not matching epochs means that the no descriptors were marked in the
    // current epoch.
    return 0;
  }

  // Encodes the number of marked descriptors for the given mark compact
  // garbage collection epoch.
  static inline int16_t encode(unsigned mark_compact_epoch, int16_t value) {
    // TODO(ulan): avoid casting to int16_t by adding support for uint16_t
    // atomics.
    return static_cast<int16_t>(
        Epoch::encode(mark_compact_epoch & Epoch::kMask) |
        Marked::encode(value));
  }
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DESCRIPTOR_ARRAY_H_
