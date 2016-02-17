// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ADDRESS_MAP_H_
#define V8_ADDRESS_MAP_H_

#include "src/assert-scope.h"
#include "src/hashmap.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class AddressMapBase {
 protected:
  static void SetValue(HashMap::Entry* entry, uint32_t v) {
    entry->value = reinterpret_cast<void*>(v);
  }

  static uint32_t GetValue(HashMap::Entry* entry) {
    return static_cast<uint32_t>(reinterpret_cast<intptr_t>(entry->value));
  }

  inline static HashMap::Entry* LookupEntry(HashMap* map, HeapObject* obj,
                                            bool insert) {
    if (insert) {
      map->LookupOrInsert(Key(obj), Hash(obj));
    }
    return map->Lookup(Key(obj), Hash(obj));
  }

 private:
  static uint32_t Hash(HeapObject* obj) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(obj->address()));
  }

  static void* Key(HeapObject* obj) {
    return reinterpret_cast<void*>(obj->address());
  }
};


class RootIndexMap : public AddressMapBase {
 public:
  explicit RootIndexMap(Isolate* isolate);

  static const int kInvalidRootIndex = -1;

  int Lookup(HeapObject* obj) {
    HashMap::Entry* entry = LookupEntry(map_, obj, false);
    if (entry) return GetValue(entry);
    return kInvalidRootIndex;
  }

 private:
  HashMap* map_;

  DISALLOW_COPY_AND_ASSIGN(RootIndexMap);
};


class BackReference {
 public:
  explicit BackReference(uint32_t bitfield) : bitfield_(bitfield) {}

  BackReference() : bitfield_(kInvalidValue) {}

  static BackReference SourceReference() { return BackReference(kSourceValue); }

  static BackReference GlobalProxyReference() {
    return BackReference(kGlobalProxyValue);
  }

  static BackReference LargeObjectReference(uint32_t index) {
    return BackReference(SpaceBits::encode(LO_SPACE) |
                         ChunkOffsetBits::encode(index));
  }

  static BackReference DummyReference() { return BackReference(kDummyValue); }

  static BackReference Reference(AllocationSpace space, uint32_t chunk_index,
                                 uint32_t chunk_offset) {
    DCHECK(IsAligned(chunk_offset, kObjectAlignment));
    DCHECK_NE(LO_SPACE, space);
    return BackReference(
        SpaceBits::encode(space) | ChunkIndexBits::encode(chunk_index) |
        ChunkOffsetBits::encode(chunk_offset >> kObjectAlignmentBits));
  }

  bool is_valid() const { return bitfield_ != kInvalidValue; }
  bool is_source() const { return bitfield_ == kSourceValue; }
  bool is_global_proxy() const { return bitfield_ == kGlobalProxyValue; }

  AllocationSpace space() const {
    DCHECK(is_valid());
    return SpaceBits::decode(bitfield_);
  }

  uint32_t chunk_offset() const {
    DCHECK(is_valid());
    return ChunkOffsetBits::decode(bitfield_) << kObjectAlignmentBits;
  }

  uint32_t large_object_index() const {
    DCHECK(is_valid());
    DCHECK(chunk_index() == 0);
    return ChunkOffsetBits::decode(bitfield_);
  }

  uint32_t chunk_index() const {
    DCHECK(is_valid());
    return ChunkIndexBits::decode(bitfield_);
  }

  uint32_t reference() const {
    DCHECK(is_valid());
    return bitfield_ & (ChunkOffsetBits::kMask | ChunkIndexBits::kMask);
  }

  uint32_t bitfield() const { return bitfield_; }

 private:
  static const uint32_t kInvalidValue = 0xFFFFFFFF;
  static const uint32_t kSourceValue = 0xFFFFFFFE;
  static const uint32_t kGlobalProxyValue = 0xFFFFFFFD;
  static const uint32_t kDummyValue = 0xFFFFFFFC;
  static const int kChunkOffsetSize = kPageSizeBits - kObjectAlignmentBits;
  static const int kChunkIndexSize = 32 - kChunkOffsetSize - kSpaceTagSize;

 public:
  static const int kMaxChunkIndex = (1 << kChunkIndexSize) - 1;

 private:
  class ChunkOffsetBits : public BitField<uint32_t, 0, kChunkOffsetSize> {};
  class ChunkIndexBits
      : public BitField<uint32_t, ChunkOffsetBits::kNext, kChunkIndexSize> {};
  class SpaceBits
      : public BitField<AllocationSpace, ChunkIndexBits::kNext, kSpaceTagSize> {
  };

  uint32_t bitfield_;
};


// Mapping objects to their location after deserialization.
// This is used during building, but not at runtime by V8.
class BackReferenceMap : public AddressMapBase {
 public:
  BackReferenceMap()
      : no_allocation_(), map_(new HashMap(HashMap::PointersMatch)) {}

  ~BackReferenceMap() { delete map_; }

  BackReference Lookup(HeapObject* obj) {
    HashMap::Entry* entry = LookupEntry(map_, obj, false);
    return entry ? BackReference(GetValue(entry)) : BackReference();
  }

  void Add(HeapObject* obj, BackReference b) {
    DCHECK(b.is_valid());
    DCHECK_NULL(LookupEntry(map_, obj, false));
    HashMap::Entry* entry = LookupEntry(map_, obj, true);
    SetValue(entry, b.bitfield());
  }

  void AddSourceString(String* string) {
    Add(string, BackReference::SourceReference());
  }

  void AddGlobalProxy(HeapObject* global_proxy) {
    Add(global_proxy, BackReference::GlobalProxyReference());
  }

 private:
  DisallowHeapAllocation no_allocation_;
  HashMap* map_;
  DISALLOW_COPY_AND_ASSIGN(BackReferenceMap);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ADDRESS_MAP_H_
