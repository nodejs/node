// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZER_COMMON_H_
#define V8_SNAPSHOT_SERIALIZER_COMMON_H_

#include "src/base/bits.h"
#include "src/base/memory.h"
#include "src/codegen/external-reference-table.h"
#include "src/common/globals.h"
#include "src/objects/visitors.h"
#include "src/sanitizer/msan.h"
#include "src/snapshot/references.h"
#include "src/utils/address-map.h"

namespace v8 {
namespace internal {

class CallHandlerInfo;
class Isolate;

class ExternalReferenceEncoder {
 public:
  class Value {
   public:
    explicit Value(uint32_t raw) : value_(raw) {}
    Value() : value_(0) {}
    static uint32_t Encode(uint32_t index, bool is_from_api) {
      return Index::encode(index) | IsFromAPI::encode(is_from_api);
    }

    bool is_from_api() const { return IsFromAPI::decode(value_); }
    uint32_t index() const { return Index::decode(value_); }

   private:
    class Index : public BitField<uint32_t, 0, 31> {};
    class IsFromAPI : public BitField<bool, 31, 1> {};
    uint32_t value_;
  };

  explicit ExternalReferenceEncoder(Isolate* isolate);
  ~ExternalReferenceEncoder();  // NOLINT (modernize-use-equals-default)

  Value Encode(Address key);
  Maybe<Value> TryEncode(Address key);

  const char* NameOfAddress(Isolate* isolate, Address address) const;

 private:
  AddressToIndexHashMap* map_;

#ifdef DEBUG
  std::vector<int> count_;
  const intptr_t* api_references_;
#endif  // DEBUG

  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceEncoder);
};

class HotObjectsList {
 public:
  HotObjectsList() : index_(0) {}

  void Add(HeapObject object) {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    circular_queue_[index_] = object;
    index_ = (index_ + 1) & kSizeMask;
  }

  HeapObject Get(int index) {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    DCHECK(!circular_queue_[index].is_null());
    return circular_queue_[index];
  }

  static const int kNotFound = -1;

  int Find(HeapObject object) {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    for (int i = 0; i < kSize; i++) {
      if (circular_queue_[i] == object) return i;
    }
    return kNotFound;
  }

  static const int kSize = 8;

 private:
  static_assert(base::bits::IsPowerOfTwo(kSize), "kSize must be power of two");
  static const int kSizeMask = kSize - 1;
  HeapObject circular_queue_[kSize];
  int index_;

  DISALLOW_COPY_AND_ASSIGN(HotObjectsList);
};

// The Serializer/Deserializer class is a common superclass for Serializer and
// Deserializer which is used to store common constants and methods used by
// both.
class SerializerDeserializer : public RootVisitor {
 public:
  static void Iterate(Isolate* isolate, RootVisitor* visitor);

 protected:
  static bool CanBeDeferred(HeapObject o);

  void RestoreExternalReferenceRedirectors(
      const std::vector<AccessorInfo>& accessor_infos);
  void RestoreExternalReferenceRedirectors(
      const std::vector<CallHandlerInfo>& call_handler_infos);

  static const int kNumberOfPreallocatedSpaces =
      static_cast<int>(SnapshotSpace::kNumberOfPreallocatedSpaces);

  static const int kNumberOfSpaces =
      static_cast<int>(SnapshotSpace::kNumberOfSpaces);

// clang-format off
#define UNUSED_SERIALIZER_BYTE_CODES(V)                           \
  V(0x06) V(0x07) V(0x0e) V(0x0f)                                 \
  /* Free range 0x26..0x2f */                                     \
  V(0x26) V(0x27)                                                 \
  V(0x28) V(0x29) V(0x2a) V(0x2b) V(0x2c) V(0x2d) V(0x2e) V(0x2f) \
  /* Free range 0x30..0x3f */                                     \
  V(0x30) V(0x31) V(0x32) V(0x33) V(0x34) V(0x35) V(0x36) V(0x37) \
  V(0x38) V(0x39) V(0x3a) V(0x3b) V(0x3c) V(0x3d) V(0x3e) V(0x3f) \
  /* Free range 0x97..0x9f */                                     \
  V(0x98) V(0x99) V(0x9a) V(0x9b) V(0x9c) V(0x9d) V(0x9e) V(0x9f) \
  /* Free range 0xa0..0xaf */                                     \
  V(0xa0) V(0xa1) V(0xa2) V(0xa3) V(0xa4) V(0xa5) V(0xa6) V(0xa7) \
  V(0xa8) V(0xa9) V(0xaa) V(0xab) V(0xac) V(0xad) V(0xae) V(0xaf) \
  /* Free range 0xb0..0xbf */                                     \
  V(0xb0) V(0xb1) V(0xb2) V(0xb3) V(0xb4) V(0xb5) V(0xb6) V(0xb7) \
  V(0xb8) V(0xb9) V(0xba) V(0xbb) V(0xbc) V(0xbd) V(0xbe) V(0xbf) \
  /* Free range 0xc0..0xcf */                                     \
  V(0xc0) V(0xc1) V(0xc2) V(0xc3) V(0xc4) V(0xc5) V(0xc6) V(0xc7) \
  V(0xc8) V(0xc9) V(0xca) V(0xcb) V(0xcc) V(0xcd) V(0xce) V(0xcf) \
  /* Free range 0xd0..0xdf */                                     \
  V(0xd0) V(0xd1) V(0xd2) V(0xd3) V(0xd4) V(0xd5) V(0xd6) V(0xd7) \
  V(0xd8) V(0xd9) V(0xda) V(0xdb) V(0xdc) V(0xdd) V(0xde) V(0xdf) \
  /* Free range 0xe0..0xef */                                     \
  V(0xe0) V(0xe1) V(0xe2) V(0xe3) V(0xe4) V(0xe5) V(0xe6) V(0xe7) \
  V(0xe8) V(0xe9) V(0xea) V(0xeb) V(0xec) V(0xed) V(0xee) V(0xef) \
  /* Free range 0xf0..0xff */                                     \
  V(0xf0) V(0xf1) V(0xf2) V(0xf3) V(0xf4) V(0xf5) V(0xf6) V(0xf7) \
  V(0xf8) V(0xf9) V(0xfa) V(0xfb) V(0xfc) V(0xfd) V(0xfe) V(0xff)
  // clang-format on

  // The static assert below will trigger when the number of preallocated spaces
  // changed. If that happens, update the kNewObject and kBackref bytecode
  // ranges in the comments below.
  STATIC_ASSERT(6 == kNumberOfSpaces);
  static const int kSpaceMask = 7;
  STATIC_ASSERT(kNumberOfSpaces <= kSpaceMask + 1);

  // First 32 root array items.
  static const int kNumberOfRootArrayConstants = 0x20;
  static const int kRootArrayConstantsMask = 0x1f;

  // 32 common raw data lengths.
  static const int kNumberOfFixedRawData = 0x20;

  // 16 repeats lengths.
  static const int kNumberOfFixedRepeat = 0x10;

  // 8 hot (recently seen or back-referenced) objects with optional skip.
  static const int kNumberOfHotObjects = 8;
  STATIC_ASSERT(kNumberOfHotObjects == HotObjectsList::kSize);
  static const int kHotObjectMask = 0x07;

  enum Bytecode {
    //
    // ---------- byte code range 0x00..0x0f ----------
    //

    // 0x00..0x05  Allocate new object, in specified space.
    kNewObject = 0x00,
    // 0x08..0x0d  Reference to previous object from specified space.
    kBackref = 0x08,

    //
    // ---------- byte code range 0x10..0x25 ----------
    //

    // Object in the partial snapshot cache.
    kPartialSnapshotCache = 0x10,
    // Root array item.
    kRootArray,
    // Object provided in the attached list.
    kAttachedReference,
    // Object in the read-only object cache.
    kReadOnlyObjectCache,
    // Do nothing, used for padding.
    kNop,
    // Move to next reserved chunk.
    kNextChunk,
    // Deferring object content.
    kDeferred,
    // 3 alignment prefixes 0x17..0x19
    kAlignmentPrefix = 0x17,
    // A tag emitted at strategic points in the snapshot to delineate sections.
    // If the deserializer does not find these at the expected moments then it
    // is an indication that the snapshot and the VM do not fit together.
    // Examine the build process for architecture, version or configuration
    // mismatches.
    kSynchronize = 0x1a,
    // Repeats of variable length.
    kVariableRepeat,
    // Used for embedder-allocated backing stores for TypedArrays.
    kOffHeapBackingStore,
    // Used for embedder-provided serialization data for embedder fields.
    kEmbedderFieldsData,
    // Raw data of variable length.
    kVariableRawCode,
    kVariableRawData,
    // Used to encode external references provided through the API.
    kApiReference,
    // External reference referenced by id.
    kExternalReference,
    // Internal reference of a code objects in code stream.
    kInternalReference,
    // In-place weak references.
    kClearedWeakReference,
    kWeakPrefix,
    // Encodes an off-heap instruction stream target.
    kOffHeapTarget,

    //
    // ---------- byte code range 0x40..0x7f ----------
    //

    // 0x40..0x5f
    kRootArrayConstants = 0x40,

    // 0x60..0x7f
    kFixedRawData = 0x60,
    kOnePointerRawData = kFixedRawData,
    kFixedRawDataStart = kFixedRawData - 1,

    //
    // ---------- byte code range 0x80..0x9f ----------
    //

    // 0x80..0x8f
    kFixedRepeat = 0x80,

    // 0x90..0x97
    kHotObject = 0x90,
  };

  //
  // Some other constants.
  //
  static const SnapshotSpace kAnyOldSpace = SnapshotSpace::kNumberOfSpaces;

  // Sentinel after a new object to indicate that double alignment is needed.
  static const int kDoubleAlignmentSentinel = 0;

  // Repeat count encoding helpers.
  static const int kFirstEncodableRepeatCount = 2;
  static const int kLastEncodableFixedRepeatCount =
      kFirstEncodableRepeatCount + kNumberOfFixedRepeat - 1;
  static const int kFirstEncodableVariableRepeatCount =
      kLastEncodableFixedRepeatCount + 1;

  // Encodes repeat count into a fixed repeat bytecode.
  static int EncodeFixedRepeat(int repeat_count) {
    DCHECK(IsInRange(repeat_count, kFirstEncodableRepeatCount,
                     kLastEncodableFixedRepeatCount));
    return kFixedRepeat + repeat_count - kFirstEncodableRepeatCount;
  }

  // Decodes repeat count from a fixed repeat bytecode.
  static int DecodeFixedRepeatCount(int bytecode) {
    DCHECK(IsInRange(bytecode, kFixedRepeat + 0,
                     kFixedRepeat + kNumberOfFixedRepeat));
    return bytecode - kFixedRepeat + kFirstEncodableRepeatCount;
  }

  // Encodes repeat count into a serialized variable repeat count value.
  static int EncodeVariableRepeatCount(int repeat_count) {
    DCHECK_LE(kFirstEncodableVariableRepeatCount, repeat_count);
    return repeat_count - kFirstEncodableVariableRepeatCount;
  }

  // Decodes repeat count from a serialized variable repeat count value.
  static int DecodeVariableRepeatCount(int value) {
    DCHECK_LE(0, value);
    return value + kFirstEncodableVariableRepeatCount;
  }

  // ---------- member variable ----------
  HotObjectsList hot_objects_;
};

class SerializedData {
 public:
  class Reservation {
   public:
    Reservation() : reservation_(0) {}
    explicit Reservation(uint32_t size)
        : reservation_(ChunkSizeBits::encode(size)) {}

    uint32_t chunk_size() const { return ChunkSizeBits::decode(reservation_); }
    bool is_last() const { return IsLastChunkBits::decode(reservation_); }

    void mark_as_last() { reservation_ |= IsLastChunkBits::encode(true); }

   private:
    uint32_t reservation_;
  };

  SerializedData(byte* data, int size)
      : data_(data), size_(size), owns_data_(false) {}
  SerializedData() : data_(nullptr), size_(0), owns_data_(false) {}
  SerializedData(SerializedData&& other) V8_NOEXCEPT
      : data_(other.data_),
        size_(other.size_),
        owns_data_(other.owns_data_) {
    // Ensure |other| will not attempt to destroy our data in destructor.
    other.owns_data_ = false;
  }

  virtual ~SerializedData() {
    if (owns_data_) DeleteArray<byte>(data_);
  }

  uint32_t GetMagicNumber() const { return GetHeaderValue(kMagicNumberOffset); }

  class ChunkSizeBits : public BitField<uint32_t, 0, 31> {};
  class IsLastChunkBits : public BitField<bool, 31, 1> {};

  static constexpr uint32_t kMagicNumberOffset = 0;
  static constexpr uint32_t kMagicNumber =
      0xC0DE0000 ^ ExternalReferenceTable::kSize;

 protected:
  void SetHeaderValue(uint32_t offset, uint32_t value) {
    base::WriteLittleEndianValue(reinterpret_cast<Address>(data_) + offset,
                                 value);
  }

  uint32_t GetHeaderValue(uint32_t offset) const {
    return base::ReadLittleEndianValue<uint32_t>(
        reinterpret_cast<Address>(data_) + offset);
  }

  void AllocateData(uint32_t size);

  void SetMagicNumber() { SetHeaderValue(kMagicNumberOffset, kMagicNumber); }

  byte* data_;
  uint32_t size_;
  bool owns_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerializedData);
};

class Checksum {
 public:
  explicit Checksum(Vector<const byte> payload) {
#ifdef MEMORY_SANITIZER
    // Computing the checksum includes padding bytes for objects like strings.
    // Mark every object as initialized in the code serializer.
    MSAN_MEMORY_IS_INITIALIZED(payload.begin(), payload.length());
#endif  // MEMORY_SANITIZER
    // Fletcher's checksum. Modified to reduce 64-bit sums to 32-bit.
    uintptr_t a = 1;
    uintptr_t b = 0;
    // TODO(jgruber, v8:9171): The following DCHECK should ideally hold since we
    // access payload through an uintptr_t pointer later on; and some
    // architectures, e.g. arm, may generate instructions that expect correct
    // alignment. However, we do not control alignment for external snapshots.
    // DCHECK(IsAligned(reinterpret_cast<intptr_t>(payload.begin()),
    //                  kIntptrSize));
    DCHECK(IsAligned(payload.length(), kIntptrSize));
    const uintptr_t* cur = reinterpret_cast<const uintptr_t*>(payload.begin());
    const uintptr_t* end = cur + payload.length() / kIntptrSize;
    while (cur < end) {
      // Unsigned overflow expected and intended.
      a += *cur++;
      b += a;
    }
#if V8_HOST_ARCH_64_BIT
    a ^= a >> 32;
    b ^= b >> 32;
#endif  // V8_HOST_ARCH_64_BIT
    a_ = static_cast<uint32_t>(a);
    b_ = static_cast<uint32_t>(b);
  }

  bool Check(uint32_t a, uint32_t b) const { return a == a_ && b == b_; }

  uint32_t a() const { return a_; }
  uint32_t b() const { return b_; }

 private:
  uint32_t a_;
  uint32_t b_;

  DISALLOW_COPY_AND_ASSIGN(Checksum);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZER_COMMON_H_
