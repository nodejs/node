// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_REFERENCES_H_
#define V8_SNAPSHOT_REFERENCES_H_

#include "src/base/hashmap.h"
#include "src/common/assert-scope.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// TODO(goszczycki): Move this somewhere every file in src/snapshot can use it.
// The spaces suported by the serializer. Spaces after LO_SPACE (NEW_LO_SPACE
// and CODE_LO_SPACE) are not supported.
enum class SnapshotSpace {
  kReadOnlyHeap = RO_SPACE,
  kNew = NEW_SPACE,
  kOld = OLD_SPACE,
  kCode = CODE_SPACE,
  kMap = MAP_SPACE,
  kLargeObject = LO_SPACE,
  kNumberOfPreallocatedSpaces = kCode + 1,
  kNumberOfSpaces = kLargeObject + 1,
  kSpecialValueSpace = kNumberOfSpaces,
  // Number of spaces which should be allocated by the heap. Eventually
  // kReadOnlyHeap will move to the end of this enum and this will be equal to
  // it.
  kNumberOfHeapSpaces = kNumberOfSpaces,
};

constexpr bool IsPreAllocatedSpace(SnapshotSpace space) {
  return static_cast<int>(space) <
         static_cast<int>(SnapshotSpace::kNumberOfPreallocatedSpaces);
}

class SerializerReference {
 private:
  enum SpecialValueType {
    kInvalidValue,
    kAttachedReference,
    kOffHeapBackingStore,
    kBuiltinReference,
  };

  STATIC_ASSERT(static_cast<int>(SnapshotSpace::kSpecialValueSpace) <
                (1 << kSpaceTagSize));

  SerializerReference(SpecialValueType type, uint32_t value)
      : bitfield_(SpaceBits::encode(SnapshotSpace::kSpecialValueSpace) |
                  SpecialValueTypeBits::encode(type)),
        value_(value) {}

 public:
  SerializerReference() : SerializerReference(kInvalidValue, 0) {}

  SerializerReference(SnapshotSpace space, uint32_t chunk_index,
                      uint32_t chunk_offset)
      : bitfield_(SpaceBits::encode(space) |
                  ChunkIndexBits::encode(chunk_index)),
        value_(chunk_offset) {}

  static SerializerReference BackReference(SnapshotSpace space,
                                           uint32_t chunk_index,
                                           uint32_t chunk_offset) {
    DCHECK(IsAligned(chunk_offset, kObjectAlignment));
    return SerializerReference(space, chunk_index, chunk_offset);
  }

  static SerializerReference MapReference(uint32_t index) {
    return SerializerReference(SnapshotSpace::kMap, 0, index);
  }

  static SerializerReference OffHeapBackingStoreReference(uint32_t index) {
    return SerializerReference(kOffHeapBackingStore, index);
  }

  static SerializerReference LargeObjectReference(uint32_t index) {
    return SerializerReference(SnapshotSpace::kLargeObject, 0, index);
  }

  static SerializerReference AttachedReference(uint32_t index) {
    return SerializerReference(kAttachedReference, index);
  }

  static SerializerReference BuiltinReference(uint32_t index) {
    return SerializerReference(kBuiltinReference, index);
  }

  bool is_valid() const {
    return SpaceBits::decode(bitfield_) != SnapshotSpace::kSpecialValueSpace ||
           SpecialValueTypeBits::decode(bitfield_) != kInvalidValue;
  }

  bool is_back_reference() const {
    return SpaceBits::decode(bitfield_) != SnapshotSpace::kSpecialValueSpace;
  }

  SnapshotSpace space() const {
    DCHECK(is_back_reference());
    return SpaceBits::decode(bitfield_);
  }

  uint32_t chunk_offset() const {
    DCHECK(is_back_reference());
    return value_;
  }

  uint32_t chunk_index() const {
    DCHECK(IsPreAllocatedSpace(space()));
    return ChunkIndexBits::decode(bitfield_);
  }

  uint32_t map_index() const {
    DCHECK_EQ(SnapshotSpace::kMap, SpaceBits::decode(bitfield_));
    return value_;
  }

  bool is_off_heap_backing_store_reference() const {
    return SpaceBits::decode(bitfield_) == SnapshotSpace::kSpecialValueSpace &&
           SpecialValueTypeBits::decode(bitfield_) == kOffHeapBackingStore;
  }

  uint32_t off_heap_backing_store_index() const {
    DCHECK(is_off_heap_backing_store_reference());
    return value_;
  }

  uint32_t large_object_index() const {
    DCHECK_EQ(SnapshotSpace::kLargeObject, SpaceBits::decode(bitfield_));
    return value_;
  }

  bool is_attached_reference() const {
    return SpaceBits::decode(bitfield_) == SnapshotSpace::kSpecialValueSpace &&
           SpecialValueTypeBits::decode(bitfield_) == kAttachedReference;
  }

  uint32_t attached_reference_index() const {
    DCHECK(is_attached_reference());
    return value_;
  }

  bool is_builtin_reference() const {
    return SpaceBits::decode(bitfield_) == SnapshotSpace::kSpecialValueSpace &&
           SpecialValueTypeBits::decode(bitfield_) == kBuiltinReference;
  }

  uint32_t builtin_index() const {
    DCHECK(is_builtin_reference());
    return value_;
  }

 private:
  using SpaceBits = BitField<SnapshotSpace, 0, kSpaceTagSize>;
  using ChunkIndexBits = SpaceBits::Next<uint32_t, 32 - kSpaceTagSize>;
  using SpecialValueTypeBits =
      SpaceBits::Next<SpecialValueType, 32 - kSpaceTagSize>;

  // We use two fields to store a reference.
  // In case of a normal back reference, the bitfield_ stores the space and
  // the chunk index. In case of special references, it uses a special value
  // for space and stores the special value type.
  uint32_t bitfield_;
  // value_ stores either chunk offset or special value.
  uint32_t value_;

  friend class SerializerReferenceMap;
};

class SerializerReferenceMap
    : public base::TemplateHashMapImpl<uintptr_t, SerializerReference,
                                       base::KeyEqualityMatcher<intptr_t>,
                                       base::DefaultAllocationPolicy> {
 public:
  using Entry = base::TemplateHashMapEntry<uintptr_t, SerializerReference>;

  SerializerReferenceMap() : attached_reference_index_(0) {}

  SerializerReference LookupReference(void* value) const {
    uintptr_t key = Key(value);
    Entry* entry = Lookup(key, Hash(key));
    if (entry == nullptr) return SerializerReference();
    return entry->value;
  }

  void Add(void* obj, SerializerReference reference) {
    DCHECK(reference.is_valid());
    DCHECK(!LookupReference(obj).is_valid());
    uintptr_t key = Key(obj);
    LookupOrInsert(key, Hash(key))->value = reference;
  }

  SerializerReference AddAttachedReference(void* attached_reference) {
    SerializerReference reference =
        SerializerReference::AttachedReference(attached_reference_index_++);
    Add(attached_reference, reference);
    return reference;
  }

 private:
  static inline uintptr_t Key(void* value) {
    return reinterpret_cast<uintptr_t>(value);
  }

  static uint32_t Hash(uintptr_t key) { return static_cast<uint32_t>(key); }

  DISALLOW_HEAP_ALLOCATION(no_allocation_)
  int attached_reference_index_;
  DISALLOW_COPY_AND_ASSIGN(SerializerReferenceMap);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_REFERENCES_H_
