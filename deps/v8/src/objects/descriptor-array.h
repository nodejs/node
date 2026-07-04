// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DESCRIPTOR_ARRAY_H_
#define V8_OBJECTS_DESCRIPTOR_ARRAY_H_

#include <atomic>

#include "src/base/bit-field.h"
#include "src/common/globals.h"
#include "src/objects/fixed-array.h"
#include "src/objects/internal-index.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

namespace compiler {
class AccessBuilder;
}  // namespace compiler

namespace maglev {
class MaglevGraphBuilder;
}  // namespace maglev

class AccessorAssembler;
class CodeStubAssembler;
class ObjectBuiltinsAssembler;
class ObjectEntriesValuesBuiltinsAssembler;
class StructBodyDescriptor;

// An EnumCache is a pair used to hold keys and indices caches.
V8_OBJECT class EnumCache : public Struct {
 public:
  inline Tagged<FixedArray> keys() const;
  inline void set_keys(Tagged<FixedArray> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> indices() const;
  inline void set_indices(Tagged<FixedArray> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(EnumCache)
  DECL_VERIFIER(EnumCache)

  using BodyDescriptor = StructBodyDescriptor;

 private:
  friend class TorqueGeneratedEnumCacheAsserts;
  friend class compiler::AccessBuilder;
  friend class maglev::MaglevGraphBuilder;
  friend class CodeStubAssembler;
  friend class AccessorAssembler;
  friend class ObjectBuiltinsAssembler;
  friend class ObjectEntriesValuesBuiltinsAssembler;
  friend class ObjectKeysAssembler;
  friend class ObjectGetOwnPropertyNamesAssembler;

  TaggedMember<FixedArray> keys_;
  TaggedMember<FixedArray> indices_;
} V8_OBJECT_END;

// A DescriptorArray is a custom array that holds instance descriptors.
// It has the following layout:
//   Header:
//     [16:0  bits]: number_of_all_descriptors (including slack)
//     [32:16 bits]: number_of_descriptors
//     [64:32 bits]: raw_gc_state (used by GC)
//     [kEnumCacheOffset]: enum cache
//   Elements:
//     [kHeaderSize + 0]: first key (and internalized String)
//     [kHeaderSize + 1]: first descriptor details (see PropertyDetails)
//     [kHeaderSize + 2]: first value for constants / Tagged<Smi>(1) when not
//     used
//   Slack:
//     [kHeaderSize + number of descriptors * 3]: start of slack
// The "value" fields store either values or field types. A field type is either
// FieldType::None(), FieldType::Any() or a weak reference to a Map. All other
// references are strong.
V8_OBJECT class DescriptorArray : public HeapObject {
 public:
  // Do linear search for small arrays, and for searches in the background
  // thread.
  static constexpr int kMaxElementsForLinearSearch = 32;

  inline int16_t number_of_all_descriptors() const;
  inline void set_number_of_all_descriptors(int16_t value, ReleaseStoreTag);
  inline int16_t number_of_descriptors() const;
  inline void set_number_of_descriptors(int16_t value);
  inline uint32_t flags(RelaxedLoadTag) const;
  inline void set_flags(uint32_t value, RelaxedStoreTag);
  inline int16_t number_of_slack_descriptors() const;
  inline int number_of_entries() const;

  enum class FastIterableState : uint8_t {
    // Descriptors are JSON fast iterable, iff all of the following conditions
    // are met:
    // - No key is a symbol.
    // - All keys are enumberable.
    // - All keys are one-byte and don't contain any character that requires
    //   escaping.
    // - All properties are located in field.
    kJsonFast = 0b00,
    kJsonSlow = 0b01,
    kUnknown = 0b11
  };

  using FastIterableBits =
      base::BitField<DescriptorArray::FastIterableState, 0, 2, uint32_t>;

  inline FastIterableState fast_iterable() const;
  inline void set_fast_iterable(FastIterableState value);
  inline void set_fast_iterable_if(FastIterableState new_value,
                                   FastIterableState if_value);

  void ClearEnumCache();
  inline void CopyEnumCacheFrom(Tagged<DescriptorArray> array);
  static void InitializeOrChangeEnumCache(
      DirectHandle<DescriptorArray> descriptors, Isolate* isolate,
      DirectHandle<FixedArray> keys, DirectHandle<FixedArray> indices,
      AllocationType allocation_if_initialize);

  inline Tagged<EnumCache> enum_cache() const;
  inline void set_enum_cache(Tagged<EnumCache> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Accessors for fetching instance descriptor at descriptor number.
  inline Tagged<Name> GetKey(InternalIndex descriptor_number) const;
  inline Tagged<Object> GetStrongValue(InternalIndex descriptor_number);
  inline Tagged<MaybeObject> GetValue(InternalIndex descriptor_number);
  inline PropertyDetails GetDetails(InternalIndex descriptor_number);
  inline int GetOffsetInWords(InternalIndex descriptor_number);
  inline Tagged<FieldType> GetFieldType(InternalIndex descriptor_number);

  // Returns true if given entry is already initialized. Useful in cases
  // when a heap stats collector might see a half-initialized descriptor.
  inline bool IsInitializedDescriptor(InternalIndex descriptor_number) const;

  inline Tagged<Name> GetSortedKey(int descriptor_number);
  inline int GetSortedKeyIndex(int descriptor_number);

  // Accessor for complete descriptor.
  inline void Set(InternalIndex descriptor_number, Descriptor* desc);
  inline void Set(InternalIndex descriptor_number, Tagged<Name> key,
                  Tagged<MaybeObject> value, PropertyDetails details);
  void Replace(InternalIndex descriptor_number, Descriptor* descriptor);

  // Generalizes constness, representation and field type of all field
  // descriptors.
  void GeneralizeAllFields();

  // Append automatically sets the enumeration index. This should only be used
  // to add descriptors in bulk at the end, followed by sorting the descriptor
  // array.
  inline void Append(Descriptor* desc);

  static DirectHandle<DescriptorArray> CopyUpTo(
      Isolate* isolate, DirectHandle<DescriptorArray> desc,
      int enumeration_index, int slack = 0);

  static DirectHandle<DescriptorArray> CopyUpToAddAttributes(
      Isolate* isolate, DirectHandle<DescriptorArray> desc,
      int enumeration_index, PropertyAttributes attributes, int slack = 0);

  // Sort the instance descriptors by the hash codes of their keys.
  inline void Sort();

  // Iterate through Name hash collisions in the descriptor array starting from
  // insertion index checking for Name collisions. Note: If we ever add binary
  // insertion for large DescriptorArrays it would need to be hardened in a
  // similar way. This function only expects to be called on Sorted
  // DescriptorArrays.
  V8_EXPORT_PRIVATE void CheckNameCollisionDuringInsertion(
      Descriptor* desc, uint32_t descriptor_hash, int insertion_index);

  // Search the instance descriptors for given name. {concurrent_search} signals
  // if we are doing the search on a background thread. If so, we will sacrifice
  // speed for thread-safety.
  V8_INLINE InternalIndex Search(Tagged<Name> name,
                                 int number_of_own_descriptors,
                                 bool concurrent_search = false);
  V8_INLINE InternalIndex Search(Tagged<Name> name, Tagged<Map> map,
                                 bool concurrent_search = false);

  // As the above, but uses DescriptorLookupCache and updates it when
  // necessary.
  V8_INLINE InternalIndex SearchWithCache(Isolate* isolate, Tagged<Name> name,
                                          Tagged<Map> map);

  bool IsEqualUpTo(Tagged<DescriptorArray> desc, int nof_descriptors);

  // Allocates a DescriptorArray, but returns the singleton
  // empty descriptor array object if number_of_descriptors is 0.
  template <typename IsolateT>
  V8_EXPORT_PRIVATE static Handle<DescriptorArray> Allocate(
      IsolateT* isolate, int nof_descriptors, int slack,
      AllocationType allocation = AllocationType::kYoung);

  void Initialize(Tagged<EnumCache> enum_cache,
                  Tagged<HeapObject> undefined_value, int nof_descriptors,
                  int slack);

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  static constexpr int SizeFor(int number_of_all_descriptors);
  static constexpr int OffsetOfDescriptorAt(int descriptor);
  inline ObjectSlot GetFirstPointerSlot();
  inline ObjectSlot GetDescriptorSlot(int descriptor);

  class BodyDescriptor;

  // Layout of descriptor.
  // Naming is consistent with Dictionary classes for easy templating.
  static const int kEntryKeyIndex = 0;
  static const int kEntryDetailsIndex = 1;
  static const int kEntryValueIndex = 2;
  static const int kEntrySize = 3;

  static const int kEntryKeyOffset = kEntryKeyIndex * kTaggedSize;
  static const int kEntryDetailsOffset = kEntryDetailsIndex * kTaggedSize;
  static const int kEntryValueOffset = kEntryValueIndex * kTaggedSize;

  static const int kHeaderSize;
  static const int kDescriptorsOffset;

  // Print all the descriptors.
  void PrintDescriptors(std::ostream& os);
  void PrintDescriptorDetails(std::ostream& os, InternalIndex descriptor,
                              PropertyDetails::PrintMode mode);

  DECL_PRINTER(DescriptorArray)
  DECL_VERIFIER(DescriptorArray)

#ifdef VERIFY_HEAP
  // Per-entry type check only (key is Name|Undefined, details is Smi|Undefined,
  // value is JSAny|Weak<Map>|AccessorInfo|AccessorPair|ClassPositions|
  // NumberDictionary). This is the hand-rolled equivalent of the old Torque-
  // generated DescriptorArrayVerify and is what test/cctest/test-verifiers.cc
  // exercises; the full DescriptorArrayVerify additionally enforces semantic
  // invariants (field values are FieldType-shaped, private keys are
  // non-enumerable, etc.) that assume a well-formed descriptor array.
  V8_EXPORT_PRIVATE void DescriptorArrayEntryTypesVerify(Isolate* isolate);
#endif

#ifdef DEBUG
  // Is the descriptor array sorted and without duplicates?
  V8_EXPORT_PRIVATE bool IsSortedNoDuplicates();

  // Are two DescriptorArrays equal?
  bool IsEqualTo(Tagged<DescriptorArray> other);
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
  V8_EXPORT_PRIVATE void SortImpl(const int len);

  inline void SetKey(InternalIndex descriptor_number, Tagged<Name> key);
  inline void SetValue(InternalIndex descriptor_number,
                       Tagged<MaybeObject> value);
  inline void SetDetails(InternalIndex descriptor_number,
                         PropertyDetails details);

  V8_INLINE InternalIndex BinarySearch(Tagged<Name> name,
                                       int number_of_own_descriptors);
  V8_INLINE InternalIndex LinearSearch(Tagged<Name> name,
                                       int number_of_own_descriptors);

  // Transfer a complete descriptor from the src descriptor array to this
  // descriptor array.
  void CopyFrom(InternalIndex index, Tagged<DescriptorArray> src);

  inline void SetSortedKey(int pointer, int descriptor_number);

  // Swap first and second descriptor.
  inline void SwapSortedKeys(int first, int second);

 public:
  // A single descriptor tuple (key, details, value). The three fields occupy
  // three adjacent tagged slots, matching the kEntry{Key,Details,Value}Offset
  // constants above. `key` and `details` are always strong after
  // initialization but may be `Undefined` in unused / slack entries;
  // `value` may hold a weak reference to a Map. See the custom
  // BodyDescriptor for GC iteration.
  struct Entry {
    TaggedMember<UnionOf<Name, Undefined>> key;
    TaggedMember<UnionOf<Smi, Undefined>> details;
    TaggedMember<UnionOf<JSAny, Weak<Map>, AccessorInfo, AccessorPair,
                         ClassPositions, NumberDictionary>>
        value;
  };

  // Declared atomic so that concurrent readers (e.g. from the marker) see a
  // consistent value during trimming in mark-compact.
  std::atomic<uint16_t> number_of_all_descriptors_;
  std::atomic<uint16_t> number_of_descriptors_;
  std::atomic<uint32_t> flags_;
  TaggedMember<EnumCache> enum_cache_;
  FLEXIBLE_ARRAY_MEMBER(Entry, entries);
} V8_OBJECT_END;

static_assert(sizeof(DescriptorArray::Entry) ==
              DescriptorArray::kEntrySize * kTaggedSize);
static_assert(offsetof(DescriptorArray::Entry, key) ==
              DescriptorArray::kEntryKeyOffset);
static_assert(offsetof(DescriptorArray::Entry, details) ==
              DescriptorArray::kEntryDetailsOffset);
static_assert(offsetof(DescriptorArray::Entry, value) ==
              DescriptorArray::kEntryValueOffset);

inline constexpr int DescriptorArray::kHeaderSize =
    OFFSET_OF_DATA_START(DescriptorArray);
inline constexpr int DescriptorArray::kDescriptorsOffset =
    OFFSET_OF_DATA_START(DescriptorArray);

constexpr int DescriptorArray::SizeFor(int number_of_all_descriptors) {
  return OFFSET_OF_DATA_START(DescriptorArray) +
         number_of_all_descriptors * kEntrySize * kTaggedSize;
}

constexpr int DescriptorArray::OffsetOfDescriptorAt(int descriptor) {
  return OFFSET_OF_DATA_START(DescriptorArray) +
         descriptor * kEntrySize * kTaggedSize;
}

static_assert(IsAligned(DescriptorArray::kHeaderSize, kTaggedSize));
static_assert(sizeof(std::atomic<uint16_t>) == 2);
static_assert(alignof(std::atomic<uint16_t>) == 2);
static_assert(sizeof(std::atomic<uint32_t>) == 4);
static_assert(alignof(std::atomic<uint32_t>) == 4);
static_assert(offsetof(DescriptorArray, number_of_all_descriptors_) ==
              sizeof(HeapObject));
static_assert(offsetof(DescriptorArray, number_of_descriptors_) ==
              sizeof(HeapObject) + sizeof(uint16_t));
static_assert(offsetof(DescriptorArray, flags_) ==
              sizeof(HeapObject) + 2 * sizeof(uint16_t));

// A DescriptorArray where all values are held strongly. Bodyless subclass with
// identical layout and BodyDescriptor. The distinct instance type routes to
// the default VisitWithBodyDescriptor path, bypassing the specialized
// MarkingVisitorBase::VisitDescriptorArray override (which runs the
// DescriptorArrayMarkingState epoch/delta protocol for incremental weak-Map
// trimming).
V8_OBJECT class StrongDescriptorArray : public DescriptorArray {
 public:
  DECL_PRINTER(StrongDescriptorArray)
  DECL_VERIFIER(StrongDescriptorArray)
} V8_OBJECT_END;

template <>
struct ObjectTraits<StrongDescriptorArray> {
  using BodyDescriptor = DescriptorArray::BodyDescriptor;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DESCRIPTOR_ARRAY_H_
