// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_INTERNAL_H_
#define INCLUDE_V8_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <atomic>
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
const intptr_t kHeapObjectReferenceTagMask = 1 << (kHeapObjectTagSize - 1);

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
#ifdef V8_ENABLE_SANDBOX
  return true;
#else
  return false;
#endif
}

// SandboxedPointers are guaranteed to point into the sandbox. This is achieved
// for example by storing them as offset rather than as raw pointers.
using SandboxedPointer_t = Address;

#ifdef V8_ENABLE_SANDBOX

// Size of the sandbox, excluding the guard regions surrounding it.
#ifdef V8_TARGET_OS_ANDROID
// On Android, most 64-bit devices seem to be configured with only 39 bits of
// virtual address space for userspace. As such, limit the sandbox to 128GB (a
// quarter of the total available address space).
constexpr size_t kSandboxSizeLog2 = 37;  // 128 GB
#else
// Everywhere else use a 1TB sandbox.
constexpr size_t kSandboxSizeLog2 = 40;  // 1 TB
#endif  // V8_TARGET_OS_ANDROID
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

static_assert(kSandboxMinimumReservationSize > kPtrComprCageReservationSize,
              "The minimum reservation size for a sandbox must be larger than "
              "the pointer compression cage contained within it.");

// The maximum buffer size allowed inside the sandbox. This is mostly dependent
// on the size of the guard regions around the sandbox: an attacker must not be
// able to construct a buffer that appears larger than the guard regions and
// thereby "reach out of" the sandbox.
constexpr size_t kMaxSafeBufferSizeForSandbox = 32ULL * GB - 1;
static_assert(kMaxSafeBufferSizeForSandbox <= kSandboxGuardRegionSize,
              "The maximum allowed buffer size must not be larger than the "
              "sandbox's guard regions");

constexpr size_t kBoundedSizeShift = 29;
static_assert(1ULL << (64 - kBoundedSizeShift) ==
                  kMaxSafeBufferSizeForSandbox + 1,
              "The maximum size of a BoundedSize must be synchronized with the "
              "kMaxSafeBufferSizeForSandbox");

#endif  // V8_ENABLE_SANDBOX

#ifdef V8_COMPRESS_POINTERS

// The size of the virtual memory reservation for an external pointer table.
// This determines the maximum number of entries in a table. Using a maximum
// size allows omitting bounds checks on table accesses if the indices are
// guaranteed (e.g. through shifting) to be below the maximum index. This
// value must be a power of two.
static const size_t kExternalPointerTableReservationSize = 512 * MB;

// The maximum number of entries in an external pointer table.
static const size_t kMaxExternalPointers =
    kExternalPointerTableReservationSize / kApiSystemPointerSize;

// The external pointer table indices stored in HeapObjects as external
// pointers are shifted to the left by this amount to guarantee that they are
// smaller than the maximum table size.
static const uint32_t kExternalPointerIndexShift = 6;
static_assert((1 << (32 - kExternalPointerIndexShift)) == kMaxExternalPointers,
              "kExternalPointerTableReservationSize and "
              "kExternalPointerIndexShift don't match");

#else  // !V8_COMPRESS_POINTERS

// Needed for the V8.SandboxedExternalPointersCount histogram.
static const size_t kMaxExternalPointers = 0;

#endif  // V8_COMPRESS_POINTERS

// A ExternalPointerHandle represents a (opaque) reference to an external
// pointer that can be stored inside the sandbox. A ExternalPointerHandle has
// meaning only in combination with an (active) Isolate as it references an
// external pointer stored in the currently active Isolate's
// ExternalPointerTable. Internally, an ExternalPointerHandles is simply an
// index into an ExternalPointerTable that is shifted to the left to guarantee
// that it is smaller than the size of the table.
using ExternalPointerHandle = uint32_t;

// ExternalPointers point to objects located outside the sandbox. When
// sandboxed external pointers are enabled, these are stored on heap as
// ExternalPointerHandles, otherwise they are simply raw pointers.
#ifdef V8_ENABLE_SANDBOX
using ExternalPointer_t = ExternalPointerHandle;
#else
using ExternalPointer_t = Address;
#endif

// When the sandbox is enabled, external pointers are stored in an external
// pointer table and are referenced from HeapObjects through an index (a
// "handle"). When stored in the table, the pointers are tagged with per-type
// tags to prevent type confusion attacks between different external objects.
// Besides type information bits, these tags also contain the GC marking bit
// which indicates whether the pointer table entry is currently alive. When a
// pointer is written into the table, the tag is ORed into the top bits. When
// that pointer is later loaded from the table, it is ANDed with the inverse of
// the expected tag. If the expected and actual type differ, this will leave
// some of the top bits of the pointer set, rendering the pointer inaccessible.
// The AND operation also removes the GC marking bit from the pointer.
//
// The tags are constructed such that UNTAG(TAG(0, T1), T2) != 0 for any two
// (distinct) tags T1 and T2. In practice, this is achieved by generating tags
// that all have the same number of zeroes and ones but different bit patterns.
// With N type tag bits, this allows for (N choose N/2) possible type tags.
// Besides the type tag bits, the tags also have the GC marking bit set so that
// the marking bit is automatically set when a pointer is written into the
// external pointer table (in which case it is clearly alive) and is cleared
// when the pointer is loaded. The exception to this is the free entry tag,
// which doesn't have the mark bit set, as the entry is not alive. This
// construction allows performing the type check and removing GC marking bits
// from the pointer in one efficient operation (bitwise AND). The number of
// available bits is limited in the following way: on x64, bits [47, 64) are
// generally available for tagging (userspace has 47 address bits available).
// On Arm64, userspace typically has a 40 or 48 bit address space. However, due
// to top-byte ignore (TBI) and memory tagging (MTE), the top byte is unusable
// for type checks as type-check failures would go unnoticed or collide with
// MTE bits. Some bits of the top byte can, however, still be used for the GC
// marking bit. The bits available for the type tags are therefore limited to
// [48, 56), i.e. (8 choose 4) = 70 different types.
// The following options exist to increase the number of possible types:
// - Using multiple ExternalPointerTables since tags can safely be reused
//   across different tables
// - Using "extended" type checks, where additional type information is stored
//   either in an adjacent pointer table entry or at the pointed-to location
// - Using a different tagging scheme, for example based on XOR which would
//   allow for 2**8 different tags but require a separate operation to remove
//   the marking bit
//
// The external pointer sandboxing mechanism ensures that every access to an
// external pointer field will result in a valid pointer of the expected type
// even in the presence of an attacker able to corrupt memory inside the
// sandbox. However, if any data related to the external object is stored
// inside the sandbox it may still be corrupted and so must be validated before
// use or moved into the external object. Further, an attacker will always be
// able to substitute different external pointers of the same type for each
// other. Therefore, code using external pointers must be written in a
// "substitution-safe" way, i.e. it must always be possible to substitute
// external pointers of the same type without causing memory corruption outside
// of the sandbox. Generally this is achieved by referencing any group of
// related external objects through a single external pointer.
//
// Currently we use bit 62 for the marking bit which should always be unused as
// it's part of the non-canonical address range. When Arm's top-byte ignore
// (TBI) is enabled, this bit will be part of the ignored byte, and we assume
// that the Embedder is not using this byte (really only this one bit) for any
// other purpose. This bit also does not collide with the memory tagging
// extension (MTE) which would use bits [56, 60).
constexpr uint64_t kExternalPointerMarkBit = 1ULL << 62;
constexpr uint64_t kExternalPointerTagMask = 0x40ff000000000000;
constexpr uint64_t kExternalPointerTagShift = 48;

// All possible 8-bit type tags.
// These are sorted so that tags can be grouped together and it can efficiently
// be checked if a tag belongs to a given group. See for example the
// IsSharedExternalPointerType routine.
constexpr uint64_t kAllExternalPointerTypeTags[] = {
    0b00001111, 0b00010111, 0b00011011, 0b00011101, 0b00011110, 0b00100111,
    0b00101011, 0b00101101, 0b00101110, 0b00110011, 0b00110101, 0b00110110,
    0b00111001, 0b00111010, 0b00111100, 0b01000111, 0b01001011, 0b01001101,
    0b01001110, 0b01010011, 0b01010101, 0b01010110, 0b01011001, 0b01011010,
    0b01011100, 0b01100011, 0b01100101, 0b01100110, 0b01101001, 0b01101010,
    0b01101100, 0b01110001, 0b01110010, 0b01110100, 0b01111000, 0b10000111,
    0b10001011, 0b10001101, 0b10001110, 0b10010011, 0b10010101, 0b10010110,
    0b10011001, 0b10011010, 0b10011100, 0b10100011, 0b10100101, 0b10100110,
    0b10101001, 0b10101010, 0b10101100, 0b10110001, 0b10110010, 0b10110100,
    0b10111000, 0b11000011, 0b11000101, 0b11000110, 0b11001001, 0b11001010,
    0b11001100, 0b11010001, 0b11010010, 0b11010100, 0b11011000, 0b11100001,
    0b11100010, 0b11100100, 0b11101000, 0b11110000};

// clang-format off
// New entries should be added with state "sandboxed".
// When adding new tags, please ensure that the code using these tags is
// "substitution-safe", i.e. still operate safely if external pointers of the
// same type are swapped by an attacker. See comment above for more details.
#define TAG(i) (kAllExternalPointerTypeTags[i])

// Shared external pointers are owned by the shared Isolate and stored in the
// shared external pointer table associated with that Isolate, where they can
// be accessed from multiple threads at the same time. The objects referenced
// in this way must therefore always be thread-safe.
#define SHARED_EXTERNAL_POINTER_TAGS(V)                        \
  V(kFirstSharedTag,                        sandboxed, TAG(0)) \
  V(kWaiterQueueNodeTag,                    sandboxed, TAG(0)) \
  V(kExternalStringResourceTag,             sandboxed, TAG(1)) \
  V(kExternalStringResourceDataTag,         sandboxed, TAG(2)) \
  V(kLastSharedTag,                         sandboxed, TAG(2))

// External pointers using these tags are kept in a per-Isolate external
// pointer table and can only be accessed when this Isolate is active.
#define PER_ISOLATE_EXTERNAL_POINTER_TAGS(V) \
  V(kForeignForeignAddressTag,              sandboxed, TAG(10)) \
  V(kNativeContextMicrotaskQueueTag,        sandboxed, TAG(11)) \
  V(kEmbedderDataSlotPayloadTag,            sandboxed, TAG(12)) \
  V(kExternalObjectValueTag,                sandboxed, TAG(13)) \
  V(kCallHandlerInfoCallbackTag,            sandboxed, TAG(14)) \
  V(kAccessorInfoGetterTag,                 sandboxed, TAG(15)) \
  V(kAccessorInfoSetterTag,                 sandboxed, TAG(16)) \
  V(kWasmInternalFunctionCallTargetTag,     sandboxed, TAG(17)) \
  V(kWasmTypeInfoNativeTypeTag,             sandboxed, TAG(18)) \
  V(kWasmExportedFunctionDataSignatureTag,  sandboxed, TAG(19)) \
  V(kWasmContinuationJmpbufTag,             sandboxed, TAG(20))

// All external pointer tags.
#define ALL_EXTERNAL_POINTER_TAGS(V) \
  SHARED_EXTERNAL_POINTER_TAGS(V)    \
  PER_ISOLATE_EXTERNAL_POINTER_TAGS(V)

// When the sandbox is enabled, external pointers marked as "sandboxed" above
// use the external pointer table (i.e. are sandboxed). This allows a gradual
// rollout of external pointer sandboxing. If the sandbox is off, no external
// pointers are sandboxed.
//
// Sandboxed external pointer tags are available when compressing pointers even
// when the sandbox is off. Some tags (e.g. kWaiterQueueNodeTag) are used
// manually with the external pointer table even when the sandbox is off to ease
// alignment requirements.
#define sandboxed(X) (X << kExternalPointerTagShift) | kExternalPointerMarkBit
#define unsandboxed(X) kUnsandboxedExternalPointerTag
#if defined(V8_COMPRESS_POINTERS)
#define EXTERNAL_POINTER_TAG_ENUM(Name, State, Bits) Name = State(Bits),
#else
#define EXTERNAL_POINTER_TAG_ENUM(Name, State, Bits) Name = unsandboxed(Bits),
#endif

#define MAKE_TAG(HasMarkBit, TypeTag)                             \
  ((static_cast<uint64_t>(TypeTag) << kExternalPointerTagShift) | \
  (HasMarkBit ? kExternalPointerMarkBit : 0))
enum ExternalPointerTag : uint64_t {
  // Empty tag value. Mostly used as placeholder.
  kExternalPointerNullTag =            MAKE_TAG(0, 0b00000000),
  // Tag to use for unsandboxed external pointers, which are still stored as
  // raw pointers on the heap.
  kUnsandboxedExternalPointerTag =     MAKE_TAG(0, 0b00000000),
  // External pointer tag that will match any external pointer. Use with care!
  kAnyExternalPointerTag =             MAKE_TAG(1, 0b11111111),
  // The free entry tag has all type bits set so every type check with a
  // different type fails. It also doesn't have the mark bit set as free
  // entries are (by definition) not alive.
  kExternalPointerFreeEntryTag =       MAKE_TAG(0, 0b11111111),
  // Evacuation entries are used during external pointer table compaction.
  kExternalPointerEvacuationEntryTag = MAKE_TAG(1, 0b11100111),

  ALL_EXTERNAL_POINTER_TAGS(EXTERNAL_POINTER_TAG_ENUM)
};

#undef MAKE_TAG
#undef unsandboxed
#undef sandboxed
#undef TAG
#undef EXTERNAL_POINTER_TAG_ENUM

// clang-format on

// True if the external pointer is sandboxed and so must be referenced through
// an external pointer table.
V8_INLINE static constexpr bool IsSandboxedExternalPointerType(
    ExternalPointerTag tag) {
  return tag != kUnsandboxedExternalPointerTag;
}

// True if the external pointer must be accessed from the shared isolate's
// external pointer table.
V8_INLINE static constexpr bool IsSharedExternalPointerType(
    ExternalPointerTag tag) {
  return tag >= kFirstSharedTag && tag <= kLastSharedTag;
}

// Sanity checks.
#define CHECK_SHARED_EXTERNAL_POINTER_TAGS(Tag, ...)    \
  static_assert(!IsSandboxedExternalPointerType(Tag) || \
                IsSharedExternalPointerType(Tag));
#define CHECK_NON_SHARED_EXTERNAL_POINTER_TAGS(Tag, ...) \
  static_assert(!IsSandboxedExternalPointerType(Tag) ||  \
                !IsSharedExternalPointerType(Tag));

SHARED_EXTERNAL_POINTER_TAGS(CHECK_SHARED_EXTERNAL_POINTER_TAGS)
PER_ISOLATE_EXTERNAL_POINTER_TAGS(CHECK_NON_SHARED_EXTERNAL_POINTER_TAGS)

#undef CHECK_NON_SHARED_EXTERNAL_POINTER_TAGS
#undef CHECK_SHARED_EXTERNAL_POINTER_TAGS

#undef SHARED_EXTERNAL_POINTER_TAGS
#undef EXTERNAL_POINTER_TAGS

// {obj} must be the raw tagged pointer representation of a HeapObject
// that's guaranteed to never be in ReadOnlySpace.
V8_EXPORT internal::Isolate* IsolateFromNeverReadOnlySpaceObject(Address obj);

// Returns if we need to throw when an error occurs. This infers the language
// mode based on the current context and the closure. This returns true if the
// language mode is strict.
V8_EXPORT bool ShouldThrowOnError(v8::internal::Isolate* isolate);
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
#ifdef V8_ENABLE_SANDBOX
  static const int kEmbedderDataSlotExternalPointerOffset = kApiTaggedSize;
#else
  static const int kEmbedderDataSlotExternalPointerOffset = 0;
#endif
  static const int kNativeContextEmbedderDataOffset = 6 * kApiTaggedSize;
  static const int kStringRepresentationAndEncodingMask = 0x0f;
  static const int kStringEncodingMask = 0x8;
  static const int kExternalTwoByteRepresentationTag = 0x02;
  static const int kExternalOneByteRepresentationTag = 0x0a;

  static const uint32_t kNumIsolateDataSlots = 4;
  static const int kStackGuardSize = 7 * kApiSystemPointerSize;
  static const int kBuiltinTier0EntryTableSize = 7 * kApiSystemPointerSize;
  static const int kBuiltinTier0TableSize = 7 * kApiSystemPointerSize;

  // ExternalPointerTable layout guarantees.
  static const int kExternalPointerTableBufferOffset = 0;
  static const int kExternalPointerTableSize = 4 * kApiSystemPointerSize;

  // IsolateData layout guarantees.
  static const int kIsolateCageBaseOffset = 0;
  static const int kIsolateStackGuardOffset =
      kIsolateCageBaseOffset + kApiSystemPointerSize;
  static const int kVariousBooleanFlagsOffset =
      kIsolateStackGuardOffset + kStackGuardSize;
  static const int kBuiltinTier0EntryTableOffset =
      kVariousBooleanFlagsOffset + kApiSystemPointerSize;
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
#ifdef V8_COMPRESS_POINTERS
  static const int kIsolateExternalPointerTableOffset =
      kIsolateLongTaskStatsCounterOffset + kApiSizetSize;
  static const int kIsolateSharedExternalPointerTableAddressOffset =
      kIsolateExternalPointerTableOffset + kExternalPointerTableSize;
  static const int kIsolateRootsOffset =
      kIsolateSharedExternalPointerTableAddressOffset + kApiSystemPointerSize;
#else
  static const int kIsolateRootsOffset =
      kIsolateLongTaskStatsCounterOffset + kApiSizetSize;
#endif

  static const int kUndefinedValueRootIndex = 4;
  static const int kTheHoleValueRootIndex = 5;
  static const int kNullValueRootIndex = 6;
  static const int kTrueValueRootIndex = 7;
  static const int kFalseValueRootIndex = 8;
  static const int kEmptyStringRootIndex = 9;

  static const int kNodeClassIdOffset = 1 * kApiSystemPointerSize;
  static const int kNodeFlagsOffset = 1 * kApiSystemPointerSize + 3;
  static const int kNodeStateMask = 0x3;
  static const int kNodeStateIsWeakValue = 2;

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

  V8_INLINE static constexpr bool CanHaveInternalField(int instance_type) {
    static_assert(kJSObjectType + 1 == kFirstJSApiObjectType);
    static_assert(kJSObjectType < kLastJSApiObjectType);
    static_assert(kFirstJSApiObjectType < kLastJSApiObjectType);
    // Check for IsJSObject() || IsJSSpecialApiObject() || IsJSApiObject()
    return instance_type == kJSSpecialApiObjectType ||
           // inlined version of base::IsInRange
           (static_cast<unsigned>(static_cast<unsigned>(instance_type) -
                                  static_cast<unsigned>(kJSObjectType)) <=
            static_cast<unsigned>(kLastJSApiObjectType - kJSObjectType));
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

#ifdef V8_ENABLE_SANDBOX
  V8_INLINE static internal::Address* GetExternalPointerTableBase(
      v8::Isolate* isolate) {
    internal::Address addr = reinterpret_cast<internal::Address>(isolate) +
                             kIsolateExternalPointerTableOffset +
                             kExternalPointerTableBufferOffset;
    return *reinterpret_cast<internal::Address**>(addr);
  }

  V8_INLINE static internal::Address* GetSharedExternalPointerTableBase(
      v8::Isolate* isolate) {
    internal::Address addr = reinterpret_cast<internal::Address>(isolate) +
                             kIsolateSharedExternalPointerTableAddressOffset;
    addr = *reinterpret_cast<internal::Address*>(addr);
    addr += kExternalPointerTableBufferOffset;
    return *reinterpret_cast<internal::Address**>(addr);
  }
#endif

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

  V8_INLINE static v8::Isolate* GetIsolateForSandbox(internal::Address obj) {
#ifdef V8_ENABLE_SANDBOX
    return reinterpret_cast<v8::Isolate*>(
        internal::IsolateFromNeverReadOnlySpaceObject(obj));
#else
    // Not used in non-sandbox mode.
    return nullptr;
#endif
  }

  template <ExternalPointerTag tag>
  V8_INLINE static internal::Address ReadExternalPointerField(
      v8::Isolate* isolate, internal::Address heap_object_ptr, int offset) {
#ifdef V8_ENABLE_SANDBOX
    if (IsSandboxedExternalPointerType(tag)) {
      // See src/sandbox/external-pointer-table-inl.h. Logic duplicated here so
      // it can be inlined and doesn't require an additional call.
      internal::Address* table =
          IsSharedExternalPointerType(tag)
              ? GetSharedExternalPointerTableBase(isolate)
              : GetExternalPointerTableBase(isolate);
      internal::ExternalPointerHandle handle =
          ReadRawField<ExternalPointerHandle>(heap_object_ptr, offset);
      uint32_t index = handle >> kExternalPointerIndexShift;
      std::atomic<internal::Address>* ptr =
          reinterpret_cast<std::atomic<internal::Address>*>(&table[index]);
      internal::Address entry =
          std::atomic_load_explicit(ptr, std::memory_order_relaxed);
      return entry & ~tag;
    }
#endif
    return ReadRawField<Address>(heap_object_ptr, offset);
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
constexpr int kGarbageCollectionReasonMaxValue = 27;

}  // namespace internal

}  // namespace v8

#endif  // INCLUDE_V8_INTERNAL_H_
