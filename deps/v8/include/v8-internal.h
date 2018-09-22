// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_INTERNAL_H_
#define INCLUDE_V8_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#include "v8-version.h"  // NOLINT(build/include)
#include "v8config.h"    // NOLINT(build/include)

namespace v8 {

class Context;
class Data;
class Isolate;

namespace internal {

class Object;

/**
 * Configuration of tagging scheme.
 */
const int kApiPointerSize = sizeof(void*);  // NOLINT
const int kApiDoubleSize = sizeof(double);  // NOLINT
const int kApiIntSize = sizeof(int);        // NOLINT
const int kApiInt64Size = sizeof(int64_t);  // NOLINT

// Tag information for HeapObject.
const int kHeapObjectTag = 1;
const int kWeakHeapObjectTag = 3;
const int kHeapObjectTagSize = 2;
const intptr_t kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1;

// Tag information for Smi.
const int kSmiTag = 0;
const int kSmiTagSize = 1;
const intptr_t kSmiTagMask = (1 << kSmiTagSize) - 1;

template <size_t tagged_ptr_size>
struct SmiTagging;

template <int kSmiShiftSize>
V8_INLINE internal::Object* IntToSmi(int value) {
  int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
  intptr_t tagged_value =
      (static_cast<intptr_t>(value) << smi_shift_bits) | kSmiTag;
  return reinterpret_cast<internal::Object*>(tagged_value);
}

// Smi constants for systems where tagged pointer is a 32-bit value.
template <>
struct SmiTagging<4> {
  enum { kSmiShiftSize = 0, kSmiValueSize = 31 };
  static int SmiShiftSize() { return kSmiShiftSize; }
  static int SmiValueSize() { return kSmiValueSize; }
  V8_INLINE static int SmiToInt(const internal::Object* value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Throw away top 32 bits and shift down (requires >> to be sign extending).
    return static_cast<int>(reinterpret_cast<intptr_t>(value)) >> shift_bits;
  }
  V8_INLINE static internal::Object* IntToSmi(int value) {
    return internal::IntToSmi<kSmiShiftSize>(value);
  }
  V8_INLINE static constexpr bool IsValidSmi(intptr_t value) {
    // To be representable as an tagged small integer, the two
    // most-significant bits of 'value' must be either 00 or 11 due to
    // sign-extension. To check this we add 01 to the two
    // most-significant bits, and check if the most-significant bit is 0
    //
    // CAUTION: The original code below:
    // bool result = ((value + 0x40000000) & 0x80000000) == 0;
    // may lead to incorrect results according to the C language spec, and
    // in fact doesn't work correctly with gcc4.1.1 in some cases: The
    // compiler may produce undefined results in case of signed integer
    // overflow. The computation must be done w/ unsigned ints.
    return static_cast<uintptr_t>(value) + 0x40000000U < 0x80000000U;
  }
};

// Smi constants for systems where tagged pointer is a 64-bit value.
template <>
struct SmiTagging<8> {
  enum { kSmiShiftSize = 31, kSmiValueSize = 32 };
  static int SmiShiftSize() { return kSmiShiftSize; }
  static int SmiValueSize() { return kSmiValueSize; }
  V8_INLINE static int SmiToInt(const internal::Object* value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Shift down and throw away top 32 bits.
    return static_cast<int>(reinterpret_cast<intptr_t>(value) >> shift_bits);
  }
  V8_INLINE static internal::Object* IntToSmi(int value) {
    return internal::IntToSmi<kSmiShiftSize>(value);
  }
  V8_INLINE static constexpr bool IsValidSmi(intptr_t value) {
    // To be representable as a long smi, the value must be a 32-bit integer.
    return (value == static_cast<int32_t>(value));
  }
};

#if V8_COMPRESS_POINTERS
static_assert(
    kApiPointerSize == kApiInt64Size,
    "Pointer compression can be enabled only for 64-bit architectures");
typedef SmiTagging<4> PlatformSmiTagging;
#else
typedef SmiTagging<kApiPointerSize> PlatformSmiTagging;
#endif

const int kSmiShiftSize = PlatformSmiTagging::kSmiShiftSize;
const int kSmiValueSize = PlatformSmiTagging::kSmiValueSize;
const int kSmiMinValue = (static_cast<unsigned int>(-1)) << (kSmiValueSize - 1);
const int kSmiMaxValue = -(kSmiMinValue + 1);
constexpr bool SmiValuesAre31Bits() { return kSmiValueSize == 31; }
constexpr bool SmiValuesAre32Bits() { return kSmiValueSize == 32; }

/**
 * This class exports constants and functionality from within v8 that
 * is necessary to implement inline functions in the v8 api.  Don't
 * depend on functions and constants defined here.
 */
class Internals {
 public:
  // These values match non-compiler-dependent values defined within
  // the implementation of v8.
  static const int kHeapObjectMapOffset = 0;
  static const int kMapInstanceTypeOffset = 1 * kApiPointerSize + kApiIntSize;
  static const int kStringResourceOffset =
      1 * kApiPointerSize + 2 * kApiIntSize;

  static const int kOddballKindOffset = 4 * kApiPointerSize + kApiDoubleSize;
  static const int kForeignAddressOffset = kApiPointerSize;
  static const int kJSObjectHeaderSize = 3 * kApiPointerSize;
  static const int kFixedArrayHeaderSize = 2 * kApiPointerSize;
  static const int kContextHeaderSize = 2 * kApiPointerSize;
  static const int kContextEmbedderDataIndex = 5;
  static const int kFullStringRepresentationMask = 0x0f;
  static const int kStringEncodingMask = 0x8;
  static const int kExternalTwoByteRepresentationTag = 0x02;
  static const int kExternalOneByteRepresentationTag = 0x0a;

  static const int kIsolateEmbedderDataOffset = 0 * kApiPointerSize;
  static const int kExternalMemoryOffset = 4 * kApiPointerSize;
  static const int kExternalMemoryLimitOffset =
      kExternalMemoryOffset + kApiInt64Size;
  static const int kExternalMemoryAtLastMarkCompactOffset =
      kExternalMemoryLimitOffset + kApiInt64Size;
  static const int kIsolateRootsOffset = kExternalMemoryLimitOffset +
                                         kApiInt64Size + kApiInt64Size +
                                         kApiPointerSize + kApiPointerSize;
  static const int kUndefinedValueRootIndex = 4;
  static const int kTheHoleValueRootIndex = 5;
  static const int kNullValueRootIndex = 6;
  static const int kTrueValueRootIndex = 7;
  static const int kFalseValueRootIndex = 8;
  static const int kEmptyStringRootIndex = 9;

  static const int kNodeClassIdOffset = 1 * kApiPointerSize;
  static const int kNodeFlagsOffset = 1 * kApiPointerSize + 3;
  static const int kNodeStateMask = 0x7;
  static const int kNodeStateIsWeakValue = 2;
  static const int kNodeStateIsPendingValue = 3;
  static const int kNodeStateIsNearDeathValue = 4;
  static const int kNodeIsIndependentShift = 3;
  static const int kNodeIsActiveShift = 4;

  static const int kFirstNonstringType = 0x80;
  static const int kOddballType = 0x83;
  static const int kForeignType = 0x87;
  static const int kJSSpecialApiObjectType = 0x410;
  static const int kJSApiObjectType = 0x420;
  static const int kJSObjectType = 0x421;

  static const int kUndefinedOddballKind = 5;
  static const int kNullOddballKind = 3;

  static const uint32_t kNumIsolateDataSlots = 4;

  // Soft limit for AdjustAmountofExternalAllocatedMemory. Trigger an
  // incremental GC once the external memory reaches this limit.
  static constexpr int kExternalAllocationSoftLimit = 64 * 1024 * 1024;

  V8_EXPORT static void CheckInitializedImpl(v8::Isolate* isolate);
  V8_INLINE static void CheckInitialized(v8::Isolate* isolate) {
#ifdef V8_ENABLE_CHECKS
    CheckInitializedImpl(isolate);
#endif
  }

  V8_INLINE static bool HasHeapObjectTag(const internal::Object* value) {
    return ((reinterpret_cast<intptr_t>(value) & kHeapObjectTagMask) ==
            kHeapObjectTag);
  }

  V8_INLINE static int SmiValue(const internal::Object* value) {
    return PlatformSmiTagging::SmiToInt(value);
  }

  V8_INLINE static internal::Object* IntToSmi(int value) {
    return PlatformSmiTagging::IntToSmi(value);
  }

  V8_INLINE static constexpr bool IsValidSmi(intptr_t value) {
    return PlatformSmiTagging::IsValidSmi(value);
  }

  V8_INLINE static int GetInstanceType(const internal::Object* obj) {
    typedef internal::Object O;
    O* map = ReadField<O*>(obj, kHeapObjectMapOffset);
    return ReadField<uint16_t>(map, kMapInstanceTypeOffset);
  }

  V8_INLINE static int GetOddballKind(const internal::Object* obj) {
    typedef internal::Object O;
    return SmiValue(ReadField<O*>(obj, kOddballKindOffset));
  }

  V8_INLINE static bool IsExternalTwoByteString(int instance_type) {
    int representation = (instance_type & kFullStringRepresentationMask);
    return representation == kExternalTwoByteRepresentationTag;
  }

  V8_INLINE static uint8_t GetNodeFlag(internal::Object** obj, int shift) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    return *addr & static_cast<uint8_t>(1U << shift);
  }

  V8_INLINE static void UpdateNodeFlag(internal::Object** obj, bool value,
                                       int shift) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    uint8_t mask = static_cast<uint8_t>(1U << shift);
    *addr = static_cast<uint8_t>((*addr & ~mask) | (value << shift));
  }

  V8_INLINE static uint8_t GetNodeState(internal::Object** obj) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    return *addr & kNodeStateMask;
  }

  V8_INLINE static void UpdateNodeState(internal::Object** obj, uint8_t value) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    *addr = static_cast<uint8_t>((*addr & ~kNodeStateMask) | value);
  }

  V8_INLINE static void SetEmbedderData(v8::Isolate* isolate, uint32_t slot,
                                        void* data) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(isolate) +
                    kIsolateEmbedderDataOffset + slot * kApiPointerSize;
    *reinterpret_cast<void**>(addr) = data;
  }

  V8_INLINE static void* GetEmbedderData(const v8::Isolate* isolate,
                                         uint32_t slot) {
    const uint8_t* addr = reinterpret_cast<const uint8_t*>(isolate) +
                          kIsolateEmbedderDataOffset + slot * kApiPointerSize;
    return *reinterpret_cast<void* const*>(addr);
  }

  V8_INLINE static internal::Object** GetRoot(v8::Isolate* isolate, int index) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(isolate) + kIsolateRootsOffset;
    return reinterpret_cast<internal::Object**>(addr + index * kApiPointerSize);
  }

  template <typename T>
  V8_INLINE static T ReadField(const internal::Object* ptr, int offset) {
    const uint8_t* addr =
        reinterpret_cast<const uint8_t*>(ptr) + offset - kHeapObjectTag;
    return *reinterpret_cast<const T*>(addr);
  }

  template <typename T>
  V8_INLINE static T ReadEmbedderData(const v8::Context* context, int index) {
    typedef internal::Object O;
    typedef internal::Internals I;
    O* ctx = *reinterpret_cast<O* const*>(context);
    int embedder_data_offset =
        I::kContextHeaderSize +
        (internal::kApiPointerSize * I::kContextEmbedderDataIndex);
    O* embedder_data = I::ReadField<O*>(ctx, embedder_data_offset);
    int value_offset =
        I::kFixedArrayHeaderSize + (internal::kApiPointerSize * index);
    return I::ReadField<T>(embedder_data, value_offset);
  }
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
  CastCheck<std::is_base_of<Data, T>::value>::Perform(data);
}

}  // namespace internal
}  // namespace v8

#endif  // INCLUDE_V8_INTERNAL_H_
