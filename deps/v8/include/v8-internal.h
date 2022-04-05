// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_INTERNAL_H_
#define INCLUDE_V8_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

#include "v8-version.h"  // NOLINT(build/include_directory)
#include "v8config.h"    // NOLINT(build/include_directory)

namespace v8 {

class Array;
class Context;
class Data;
class Isolate;
template <typename T>
class Local;

namespace internal {

class Isolate;

typedef uintptr_t Address;
static const Address kNullAddress = 0;

constexpr int KB = 1024;
constexpr int MB = KB * 1024;
constexpr int GB = MB * 1024;
#ifdef V8_TARGET_ARCH_X64
constexpr size_t TB = size_t{GB} * 1024;
#endif

/**
 * Configuration of tagging scheme.
 */
const int kApiSystemPointerSize = sizeof(void*);
const int kApiDoubleSize = sizeof(double);
const int kApiInt32Size = sizeof(int32_t);
const int kApiInt64Size = sizeof(int64_t);
const int kApiSizetSize = sizeof(size_t);

// Tag information for HeapObject.
const int kHeapObjectTag = 1;
const int kWeakHeapObjectTag = 3;
const int kHeapObjectTagSize = 2;
const intptr_t kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1;

// Tag information for fowarding pointers stored in object headers.
// 0b00 at the lowest 2 bits in the header indicates that the map word is a
// forwarding pointer.
const int kForwardingTag = 0;
const int kForwardingTagSize = 2;
const intptr_t kForwardingTagMask = (1 << kForwardingTagSize) - 1;

// Tag information for Smi.
const int kSmiTag = 0;
const int kSmiTagSize = 1;
const intptr_t kSmiTagMask = (1 << kSmiTagSize) - 1;

template <size_t tagged_ptr_size>
struct SmiTagging;

constexpr intptr_t kIntptrAllBitsSet = intptr_t{-1};
constexpr uintptr_t kUintptrAllBitsSet =
    static_cast<uintptr_t>(kIntptrAllBitsSet);

// Smi constants for systems where tagged pointer is a 32-bit value.
template <>
struct SmiTagging<4> {
  enum { kSmiShiftSize = 0, kSmiValueSize = 31 };

  static constexpr intptr_t kSmiMinValue =
      static_cast<intptr_t>(kUintptrAllBitsSet << (kSmiValueSize - 1));
  static constexpr intptr_t kSmiMaxValue = -(kSmiMinValue + 1);

  V8_INLINE static int SmiToInt(const internal::Address value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Truncate and shift down (requires >> to be sign extending).
    return static_cast<int32_t>(static_cast<uint32_t>(value)) >> shift_bits;
  }
  V8_INLINE static constexpr bool IsValidSmi(intptr_t value) {
    // Is value in range [kSmiMinValue, kSmiMaxValue].
    // Use unsigned operations in order to avoid undefined behaviour in case of
    // signed integer overflow.
    return (static_cast<uintptr_t>(value) -
            static_cast<uintptr_t>(kSmiMinValue)) <=
           (static_cast<uintptr_t>(kSmiMaxValue) -
            static_cast<uintptr_t>(kSmiMinValue));
  }
};

// Smi constants for systems where tagged pointer is a 64-bit value.
template <>
struct SmiTagging<8> {
  enum { kSmiShiftSize = 31, kSmiValueSize = 32 };

  static constexpr intptr_t kSmiMinValue =
      static_cast<intptr_t>(kUintptrAllBitsSet << (kSmiValueSize - 1));
  static constexpr intptr_t kSmiMaxValue = -(kSmiMinValue + 1);

  V8_INLINE static int SmiToInt(const internal::Address value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Shift down and throw away top 32 bits.
    return static_cast<int>(static_cast<intptr_t>(value) >> shift_bits);
  }
  V8_INLINE static constexpr bool IsValidSmi(intptr_t value) {
    // To be representable as a long smi, the value must be a 32-bit integer.
    return (value == static_cast<int32_t>(value));
  }
};

#ifdef V8_COMPRESS_POINTERS
// See v8:7703 or src/common/ptr-compr-inl.h for details about pointer
// compression.
constexpr size_t kPtrComprCageReservationSize = size_t{1} << 32;
constexpr size_t kPtrComprCageBaseAlignment = size_t{1} << 32;

static_assert(
    kApiSystemPointerSize == kApiInt64Size,
    "Pointer compression can be enabled only for 64-bit architectures");
const int kApiTaggedSize = kApiInt32Size;
#else
const int kApiTaggedSize = kApiSystemPointerSize;
#endif

constexpr bool PointerCompressionIsEnabled() {
  return kApiTaggedSize != kApiSystemPointerSize;
}

#ifdef V8_31BIT_SMIS_ON_64BIT_ARCH
using PlatformSmiTagging = SmiTagging<kApiInt32Size>;
#else
using PlatformSmiTagging = SmiTagging<kApiTaggedSize>;
#endif

// TODO(ishell): Consinder adding kSmiShiftBits = kSmiShiftSize + kSmiTagSize
// since it's used much more often than the inividual constants.
const int kSmiShiftSize = PlatformSmiTagging::kSmiShiftSize;
const int kSmiValueSize = PlatformSmiTagging::kSmiValueSize;
const int kSmiMinValue = static_cast<int>(PlatformSmiTagging::kSmiMinValue);
const int kSmiMaxValue = static_cast<int>(PlatformSmiTagging::kSmiMaxValue);
constexpr bool SmiValuesAre31Bits() { return kSmiValueSize == 31; }
constexpr bool SmiValuesAre32Bits() { return kSmiValueSize == 32; }

V8_INLINE static constexpr internal::Address IntToSmi(int value) {
  return (static_cast<Address>(value) << (kSmiTagSize + kSmiShiftSize)) |
         kSmiTag;
}

/*
 * Sandbox related types, constants, and functions.
 */
constexpr bool SandboxIsEnabled() {
#ifdef V8_SANDBOX
  return true;
#else
  return false;
#endif
}

constexpr bool SandboxedExternalPointersAreEnabled() {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  return true;
#else
  return false;
#endif
}

// SandboxedPointers are guaranteed to point into the sandbox. This is achieved
// for example by storing them as offset rather than as raw pointers.
using SandboxedPointer_t = Address;

// ExternalPointers point to objects located outside the sandbox. When sandboxed
// external pointers are enabled, these are stored in an external pointer table
// and referenced from HeapObjects through indices.
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
using ExternalPointer_t = uint32_t;
#else
using ExternalPointer_t = Address;
#endif

#ifdef V8_SANDBOX_IS_AVAILABLE

// Size of the sandbox, excluding the guard regions surrounding it.
constexpr size_t kSandboxSizeLog2 = 40;  // 1 TB
constexpr size_t kSandboxSize = 1ULL << kSandboxSizeLog2;

// Required alignment of the sandbox. For simplicity, we require the
// size of the guard regions to be a multiple of this, so that this specifies
// the alignment of the sandbox including and excluding surrounding guard
// regions. The alignment requirement is due to the pointer compression cage
// being located at the start of the sandbox.
constexpr size_t kSandboxAlignment = kPtrComprCageBaseAlignment;

// Sandboxed pointers are stored inside the heap as offset from the sandbox
// base shifted to the left. This way, it is guaranteed that the offset is
// smaller than the sandbox size after shifting it to the right again. This
// constant specifies the shift amount.
constexpr uint64_t kSandboxedPointerShift = 64 - kSandboxSizeLog2;

// Size of the guard regions surrounding the sandbox. This assumes a worst-case
// scenario of a 32-bit unsigned index used to access an array of 64-bit
// values.
constexpr size_t kSandboxGuardRegionSize = 32ULL * GB;

static_assert((kSandboxGuardRegionSize % kSandboxAlignment) == 0,
              "The size of the guard regions around the sandbox must be a "
              "multiple of its required alignment.");

// Minimum size of the sandbox, excluding the guard regions surrounding it. If
// the virtual memory reservation for the sandbox fails, its size is currently
// halved until either the reservation succeeds or the minimum size is reached.
// A minimum of 32GB allows the 4GB pointer compression region as well as the
// ArrayBuffer partition and two 10GB Wasm memory cages to fit into the
// sandbox. 32GB should also be the minimum possible size of the userspace
// address space as there are some machine configurations with only 36 virtual
// address bits.
constexpr size_t kSandboxMinimumSize = 32ULL * GB;

static_assert(kSandboxMinimumSize <= kSandboxSize,
              "The minimal size of the sandbox must be smaller or equal to the "
              "regular size.");

// On OSes where reserving virtual memory is too expensive to reserve the
// entire address space backing the sandbox, notably Windows pre 8.1, we create
// a partially reserved sandbox that doesn't actually reserve most of the
// memory, and so doesn't have the desired security properties as unrelated
// memory allocations could end up inside of it, but which still ensures that
// objects that should be located inside the sandbox are allocated within
// kSandboxSize bytes from the start of the sandbox. The minimum size of the
// region that is actually reserved for such a sandbox is specified by this
// constant and should be big enough to contain the pointer compression cage as
// well as the ArrayBuffer partition.
constexpr size_t kSandboxMinimumReservationSize = 8ULL * GB;

static_assert(kSandboxMinimumSize > kPtrComprCageReservationSize,
              "The sandbox must be larger than the pointer compression cage "
              "contained within it.");
static_assert(kSandboxMinimumReservationSize > kPtrComprCageReservationSize,
              "The minimum reservation size for a sandbox must be larger than "
              "the pointer compression cage contained within it.");

// For now, even if the sandbox is enabled, we still allow backing stores to be
// allocated outside of it as fallback. This will simplify the initial rollout.
// However, if sandboxed pointers are also enabled, we must always place
// backing stores inside the sandbox as they will be referenced though them.
#ifdef V8_SANDBOXED_POINTERS
constexpr bool kAllowBackingStoresOutsideSandbox = false;
#else
constexpr bool kAllowBackingStoresOutsideSandbox = true;
#endif  // V8_SANDBOXED_POINTERS

// The size of the virtual memory reservation for an external pointer table.
// This determines the maximum number of entries in a table. Using a maximum
// size allows omitting bounds checks on table accesses if the indices are
// guaranteed (e.g. through shifting) to be below the maximum index. This
// value must be a power of two.
static const size_t kExternalPointerTableReservationSize = 128 * MB;

// The maximum number of entries in an external pointer table.
static const size_t kMaxSandboxedExternalPointers =
    kExternalPointerTableReservationSize / kApiSystemPointerSize;

// The external pointer table indices stored in HeapObjects as external
// pointers are shifted to the left by this amount to guarantee that they are
// smaller than the maximum table size.
static const uint32_t kExternalPointerIndexShift = 8;
static_assert((1 << (32 - kExternalPointerIndexShift)) ==
                  kMaxSandboxedExternalPointers,
              "kExternalPointerTableReservationSize and "
              "kExternalPointerIndexShift don't match");

#endif  // V8_SANDBOX_IS_AVAILABLE

// If sandboxed external pointers are enabled, these tag values will be ORed
// with the external pointers in the external pointer table to prevent use of
// pointers of the wrong type. When a pointer is loaded, it is ANDed with the
// inverse of the expected type's tag. The tags are constructed in a way that
// guarantees that a failed type check will result in one or more of the top
// bits of the pointer to be set, rendering the pointer inacessible. Besides
// the type tag bits (48 through 62), the tags also have the GC mark bit (63)
// set, so that the mark bit is automatically set when a pointer is written
// into the external pointer table (in which case it is clearly alive) and is
// cleared when the pointer is loaded. The exception to this is the free entry
// tag, which doesn't have the mark bit set, as the entry is not alive. This
// construction allows performing the type check and removing GC marking bits
// (the MSB) from the pointer at the same time.
// Note: this scheme assumes a 48-bit address space and will likely break if
// more virtual address bits are used.
constexpr uint64_t kExternalPointerTagMask = 0xffff000000000000;
constexpr uint64_t kExternalPointerTagShift = 48;
#define MAKE_TAG(v) (static_cast<uint64_t>(v) << kExternalPointerTagShift)
// clang-format off
enum ExternalPointerTag : uint64_t {
  kExternalPointerNullTag =         MAKE_TAG(0b0000000000000000),
  kExternalPointerFreeEntryTag =    MAKE_TAG(0b0111111110000000),
  kExternalStringResourceTag =      MAKE_TAG(0b1000000011111111),
  kExternalStringResourceDataTag =  MAKE_TAG(0b1000000101111111),
  kForeignForeignAddressTag =       MAKE_TAG(0b1000000110111111),
  kNativeContextMicrotaskQueueTag = MAKE_TAG(0b1000000111011111),
  kEmbedderDataSlotPayloadTag =     MAKE_TAG(0b1000000111101111),
  kCodeEntryPointTag =              MAKE_TAG(0b1000000111110111),
  kExternalObjectValueTag =         MAKE_TAG(0b1000000111111011),
};
// clang-format on
#undef MAKE_TAG

// Converts encoded external pointer to address.
V8_EXPORT Address DecodeExternalPointerImpl(const Isolate* isolate,
                                            ExternalPointer_t pointer,
                                            ExternalPointerTag tag);

// {obj} must be the raw tagged pointer representation of a HeapObject
// that's guaranteed to never be in ReadOnlySpace.
V8_EXPORT internal::Isolate* IsolateFromNeverReadOnlySpaceObject(Address obj);

// Returns if we need to throw when an error occurs. This infers the language
// mode based on the current context and the closure. This returns true if the
// language mode is strict.
V8_EXPORT bool ShouldThrowOnError(v8::internal::Isolate* isolate);

V8_EXPORT bool CanHaveInternalField(int instance_type);

/**
 * This class exports constants and functionality from within v8 that
 * is necessary to implement inline functions in the v8 api.  Don't
 * depend on functions and constants defined here.
 */
class Internals {
#ifdef V8_MAP_PACKING
  V8_INLINE static constexpr internal::Address UnpackMapWord(
      internal::Address mapword) {
    // TODO(wenyuzhao): Clear header metadata.
    return mapword ^ kMapWordXorMask;
  }
#endif

 public:
  // These values match non-compiler-dependent values defined within
  // the implementation of v8.
  static const int kHeapObjectMapOffset = 0;
  static const int kMapInstanceTypeOffset = 1 * kApiTaggedSize + kApiInt32Size;
  static const int kStringResourceOffset =
      1 * kApiTaggedSize + 2 * kApiInt32Size;

  static const int kOddballKindOffset = 4 * kApiTaggedSize + kApiDoubleSize;
  static const int kJSObjectHeaderSize = 3 * kApiTaggedSize;
  static const int kFixedArrayHeaderSize = 2 * kApiTaggedSize;
  static const int kEmbedderDataArrayHeaderSize = 2 * kApiTaggedSize;
  static const int kEmbedderDataSlotSize = kApiSystemPointerSize;
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  static const int kEmbedderDataSlotRawPayloadOffset = kApiTaggedSize;
#endif
  static const int kNativeContextEmbedderDataOffset = 6 * kApiTaggedSize;
  static const int kStringRepresentationAndEncodingMask = 0x0f;
  static const int kStringEncodingMask = 0x8;
  static const int kExternalTwoByteRepresentationTag = 0x02;
  static const int kExternalOneByteRepresentationTag = 0x0a;

  static const uint32_t kNumIsolateDataSlots = 4;
  static const int kStackGuardSize = 7 * kApiSystemPointerSize;
  static const int kBuiltinTier0EntryTableSize = 9 * kApiSystemPointerSize;
  static const int kBuiltinTier0TableSize = 9 * kApiSystemPointerSize;

  // IsolateData layout guarantees.
  static const int kIsolateCageBaseOffset = 0;
  static const int kIsolateStackGuardOffset =
      kIsolateCageBaseOffset + kApiSystemPointerSize;
  static const int kBuiltinTier0EntryTableOffset =
      kIsolateStackGuardOffset + kStackGuardSize;
  static const int kBuiltinTier0TableOffset =
      kBuiltinTier0EntryTableOffset + kBuiltinTier0EntryTableSize;
  static const int kIsolateEmbedderDataOffset =
      kBuiltinTier0TableOffset + kBuiltinTier0TableSize;
  static const int kIsolateFastCCallCallerFpOffset =
      kIsolateEmbedderDataOffset + kNumIsolateDataSlots * kApiSystemPointerSize;
  static const int kIsolateFastCCallCallerPcOffset =
      kIsolateFastCCallCallerFpOffset + kApiSystemPointerSize;
  static const int kIsolateFastApiCallTargetOffset =
      kIsolateFastCCallCallerPcOffset + kApiSystemPointerSize;
  static const int kIsolateLongTaskStatsCounterOffset =
      kIsolateFastApiCallTargetOffset + kApiSystemPointerSize;
  static const int kIsolateRootsOffset =
      kIsolateLongTaskStatsCounterOffset + kApiSizetSize;

  static const int kExternalPointerTableBufferOffset = 0;
  static const int kExternalPointerTableCapacityOffset =
      kExternalPointerTableBufferOffset + kApiSystemPointerSize;
  static const int kExternalPointerTableFreelistHeadOffset =
      kExternalPointerTableCapacityOffset + kApiInt32Size;

  static const int kUndefinedValueRootIndex = 4;
  static const int kTheHoleValueRootIndex = 5;
  static const int kNullValueRootIndex = 6;
  static const int kTrueValueRootIndex = 7;
  static const int kFalseValueRootIndex = 8;
  static const int kEmptyStringRootIndex = 9;

  static const int kNodeClassIdOffset = 1 * kApiSystemPointerSize;
  static const int kNodeFlagsOffset = 1 * kApiSystemPointerSize + 3;
  static const int kNodeStateMask = 0x7;
  static const int kNodeStateIsWeakValue = 2;
  static const int kNodeStateIsPendingValue = 3;

  static const int kFirstNonstringType = 0x80;
  static const int kOddballType = 0x83;
  static const int kForeignType = 0xcc;
  static const int kJSSpecialApiObjectType = 0x410;
  static const int kJSObjectType = 0x421;
  static const int kFirstJSApiObjectType = 0x422;
  static const int kLastJSApiObjectType = 0x80A;

  static const int kUndefinedOddballKind = 5;
  static const int kNullOddballKind = 3;

  // Constants used by PropertyCallbackInfo to check if we should throw when an
  // error occurs.
  static const int kThrowOnError = 0;
  static const int kDontThrow = 1;
  static const int kInferShouldThrowMode = 2;

  // Soft limit for AdjustAmountofExternalAllocatedMemory. Trigger an
  // incremental GC once the external memory reaches this limit.
  static constexpr int kExternalAllocationSoftLimit = 64 * 1024 * 1024;

#ifdef V8_MAP_PACKING
  static const uintptr_t kMapWordMetadataMask = 0xffffULL << 48;
  // The lowest two bits of mapwords are always `0b10`
  static const uintptr_t kMapWordSignature = 0b10;
  // XORing a (non-compressed) map with this mask ensures that the two
  // low-order bits are 0b10. The 0 at the end makes this look like a Smi,
  // although real Smis have all lower 32 bits unset. We only rely on these
  // values passing as Smis in very few places.
  static const int kMapWordXorMask = 0b11;
#endif

  V8_EXPORT static void CheckInitializedImpl(v8::Isolate* isolate);
  V8_INLINE static void CheckInitialized(v8::Isolate* isolate) {
#ifdef V8_ENABLE_CHECKS
    CheckInitializedImpl(isolate);
#endif
  }

  V8_INLINE static bool HasHeapObjectTag(const internal::Address value) {
    return (value & kHeapObjectTagMask) == static_cast<Address>(kHeapObjectTag);
  }

  V8_INLINE static int SmiValue(const internal::Address value) {
    return PlatformSmiTagging::SmiToInt(value);
  }

  V8_INLINE static constexpr internal::Address IntToSmi(int value) {
    return internal::IntToSmi(value);
  }

  V8_INLINE static constexpr bool IsValidSmi(intptr_t value) {
    return PlatformSmiTagging::IsValidSmi(value);
  }

  V8_INLINE static int GetInstanceType(const internal::Address obj) {
    typedef internal::Address A;
    A map = ReadTaggedPointerField(obj, kHeapObjectMapOffset);
#ifdef V8_MAP_PACKING
    map = UnpackMapWord(map);
#endif
    return ReadRawField<uint16_t>(map, kMapInstanceTypeOffset);
  }

  V8_INLINE static int GetOddballKind(const internal::Address obj) {
    return SmiValue(ReadTaggedSignedField(obj, kOddballKindOffset));
  }

  V8_INLINE static bool IsExternalTwoByteString(int instance_type) {
    int representation = (instance_type & kStringRepresentationAndEncodingMask);
    return representation == kExternalTwoByteRepresentationTag;
  }

  V8_INLINE static uint8_t GetNodeFlag(internal::Address* obj, int shift) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    return *addr & static_cast<uint8_t>(1U << shift);
  }

  V8_INLINE static void UpdateNodeFlag(internal::Address* obj, bool value,
                                       int shift) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    uint8_t mask = static_cast<uint8_t>(1U << shift);
    *addr = static_cast<uint8_t>((*addr & ~mask) | (value << shift));
  }

  V8_INLINE static uint8_t GetNodeState(internal::Address* obj) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    return *addr & kNodeStateMask;
  }

  V8_INLINE static void UpdateNodeState(internal::Address* obj, uint8_t value) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    *addr = static_cast<uint8_t>((*addr & ~kNodeStateMask) | value);
  }

  V8_INLINE static void SetEmbedderData(v8::Isolate* isolate, uint32_t slot,
                                        void* data) {
    internal::Address addr = reinterpret_cast<internal::Address>(isolate) +
                             kIsolateEmbedderDataOffset +
                             slot * kApiSystemPointerSize;
    *reinterpret_cast<void**>(addr) = data;
  }

  V8_INLINE static void* GetEmbedderData(const v8::Isolate* isolate,
                                         uint32_t slot) {
    internal::Address addr = reinterpret_cast<internal::Address>(isolate) +
                             kIsolateEmbedderDataOffset +
                             slot * kApiSystemPointerSize;
    return *reinterpret_cast<void* const*>(addr);
  }

  V8_INLINE static void IncrementLongTasksStatsCounter(v8::Isolate* isolate) {
    internal::Address addr = reinterpret_cast<internal::Address>(isolate) +
                             kIsolateLongTaskStatsCounterOffset;
    ++(*reinterpret_cast<size_t*>(addr));
  }

  V8_INLINE static internal::Address* GetRoot(v8::Isolate* isolate, int index) {
    internal::Address addr = reinterpret_cast<internal::Address>(isolate) +
                             kIsolateRootsOffset +
                             index * kApiSystemPointerSize;
    return reinterpret_cast<internal::Address*>(addr);
  }

  template <typename T>
  V8_INLINE static T ReadRawField(internal::Address heap_object_ptr,
                                  int offset) {
    internal::Address addr = heap_object_ptr + offset - kHeapObjectTag;
#ifdef V8_COMPRESS_POINTERS
    if (sizeof(T) > kApiTaggedSize) {
      // TODO(ishell, v8:8875): When pointer compression is enabled 8-byte size
      // fields (external pointers, doubles and BigInt data) are only
      // kTaggedSize aligned so we have to use unaligned pointer friendly way of
      // accessing them in order to avoid undefined behavior in C++ code.
      T r;
      memcpy(&r, reinterpret_cast<void*>(addr), sizeof(T));
      return r;
    }
#endif
    return *reinterpret_cast<const T*>(addr);
  }

  V8_INLINE static internal::Address ReadTaggedPointerField(
      internal::Address heap_object_ptr, int offset) {
#ifdef V8_COMPRESS_POINTERS
    uint32_t value = ReadRawField<uint32_t>(heap_object_ptr, offset);
    internal::Address base =
        GetPtrComprCageBaseFromOnHeapAddress(heap_object_ptr);
    return base + static_cast<internal::Address>(static_cast<uintptr_t>(value));
#else
    return ReadRawField<internal::Address>(heap_object_ptr, offset);
#endif
  }

  V8_INLINE static internal::Address ReadTaggedSignedField(
      internal::Address heap_object_ptr, int offset) {
#ifdef V8_COMPRESS_POINTERS
    uint32_t value = ReadRawField<uint32_t>(heap_object_ptr, offset);
    return static_cast<internal::Address>(static_cast<uintptr_t>(value));
#else
    return ReadRawField<internal::Address>(heap_object_ptr, offset);
#endif
  }

  V8_INLINE static internal::Isolate* GetIsolateForSandbox(
      internal::Address obj) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
    return internal::IsolateFromNeverReadOnlySpaceObject(obj);
#else
    // Not used in non-sandbox mode.
    return nullptr;
#endif
  }

  V8_INLINE static Address DecodeExternalPointer(
      const Isolate* isolate, ExternalPointer_t encoded_pointer,
      ExternalPointerTag tag) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
    return internal::DecodeExternalPointerImpl(isolate, encoded_pointer, tag);
#else
    return encoded_pointer;
#endif
  }

  V8_INLINE static internal::Address ReadExternalPointerField(
      internal::Isolate* isolate, internal::Address heap_object_ptr, int offset,
      ExternalPointerTag tag) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
    internal::ExternalPointer_t encoded_value =
        ReadRawField<uint32_t>(heap_object_ptr, offset);
    // We currently have to treat zero as nullptr in embedder slots.
    return encoded_value ? DecodeExternalPointer(isolate, encoded_value, tag)
                         : 0;
#else
    return ReadRawField<Address>(heap_object_ptr, offset);
#endif
  }

#ifdef V8_COMPRESS_POINTERS
  V8_INLINE static internal::Address GetPtrComprCageBaseFromOnHeapAddress(
      internal::Address addr) {
    return addr & -static_cast<intptr_t>(kPtrComprCageBaseAlignment);
  }

  V8_INLINE static internal::Address DecompressTaggedAnyField(
      internal::Address heap_object_ptr, uint32_t value) {
    internal::Address base =
        GetPtrComprCageBaseFromOnHeapAddress(heap_object_ptr);
    return base + static_cast<internal::Address>(static_cast<uintptr_t>(value));
  }

#endif  // V8_COMPRESS_POINTERS
};

// Only perform cast check for types derived from v8::Data since
// other types do not implement the Cast method.
template <bool PerformCheck>
struct CastCheck {
  template <class T>
  static void Perform(T* data);
};

template <>
template <class T>
void CastCheck<true>::Perform(T* data) {
  T::Cast(data);
}

template <>
template <class T>
void CastCheck<false>::Perform(T* data) {}

template <class T>
V8_INLINE void PerformCastCheck(T* data) {
  CastCheck<std::is_base_of<Data, T>::value &&
            !std::is_same<Data, std::remove_cv_t<T>>::value>::Perform(data);
}

// A base class for backing stores, which is needed due to vagaries of
// how static casts work with std::shared_ptr.
class BackingStoreBase {};

// The maximum value in enum GarbageCollectionReason, defined in heap.h.
// This is needed for histograms sampling garbage collection reasons.
constexpr int kGarbageCollectionReasonMaxValue = 25;

}  // namespace internal

}  // namespace v8

#endif  // INCLUDE_V8_INTERNAL_H_
