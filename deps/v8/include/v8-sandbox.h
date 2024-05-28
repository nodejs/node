// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_SANDBOX_H_
#define INCLUDE_V8_SANDBOX_H_

#include <cstdint>

#include "v8-internal.h"  // NOLINT(build/include_directory)
#include "v8config.h"     // NOLINT(build/include_directory)

namespace v8 {

/**
 * A pointer tag used for wrapping and unwrapping `CppHeap` pointers as used
 * with JS API wrapper objects that rely on `v8::Object::Wrap()` and
 * `v8::Object::Unwrap()`.
 *
 * The CppHeapPointers use a range-based type checking scheme, where on access
 * to a pointer, the actual type of the pointer is checked to be within a
 * specified range of types. This allows supporting type hierarchies, where a
 * type check for a supertype must succeed for any subtype.
 *
 * The tag is currently in practice limited to 15 bits since it needs to fit
 * together with a marking bit into the unused parts of a pointer (the top 16
 * bits).
 */
enum class CppHeapPointerTag : uint16_t {
  kFirstTag = 0,
  kNullTag = 0,

  // The lower type ids are reserved for the embedder to assign. For that, the
  // main requirement is that all (transitive) child classes of a given parent
  // class have type ids in the same range, and that there are no unrelated
  // types in that range. For example, given the following type hierarchy:
  //
  //          A     F
  //         / \
  //        B   E
  //       / \
  //      C   D
  //
  // a potential type id assignment that satistifes these requirements is
  // {C: 0, D: 1, B: 2, A: 3, E: 4, F: 5}. With that, the type check for type A
  // would check for the range [0, 4], while the check for B would check range
  // [0, 2], and for F it would simply check [5, 5].
  //
  // In addition, there is an option for performance tweaks: if the size of the
  // type range corresponding to a supertype is a power of two and starts at a
  // power of two (e.g. [0x100, 0x13f]), then the compiler can often optimize
  // the type check to use even fewer instructions (essentially replace a AND +
  // SUB with a single AND).

  kDefaultTag = 0x7000,

  kZappedEntryTag = 0x7ffd,
  kEvacuationEntryTag = 0x7ffe,
  kFreeEntryTag = 0x7fff,
  // The tags are limited to 15 bits, so the last tag is 0x7fff.
  kLastTag = 0x7fff,
};

// Convenience struct to represent tag ranges. This is used for type checks
// against supertypes, which cover a range of types (their subtypes).
// Both the lower- and the upper bound are inclusive. In other words, this
// struct represents the range [lower_bound, upper_bound].
struct CppHeapPointerTagRange {
  constexpr CppHeapPointerTagRange(CppHeapPointerTag lower,
                                   CppHeapPointerTag upper)
      : lower_bound(lower), upper_bound(upper) {}
  CppHeapPointerTag lower_bound;
  CppHeapPointerTag upper_bound;

  // Check whether the tag of the given CppHeapPointerTable entry is within
  // this range. This method encodes implementation details of the
  // CppHeapPointerTable, which is necessary as it is used by
  // ReadCppHeapPointerField below.
  // Returns true if the check is successful and the tag of the given entry is
  // within this range, false otherwise.
  bool CheckTagOf(uint64_t entry) {
    // Note: the cast to uint32_t is important here. Otherwise, the uint16_t's
    // would be promoted to int in the range check below, which would result in
    // undefined behavior (signed integer undeflow) if the actual value is less
    // than the lower bound. Then, the compiler would take advantage of the
    // undefined behavior and turn the range check into a simple
    // `actual_tag <= last_tag` comparison, which is incorrect.
    uint32_t actual_tag = static_cast<uint16_t>(entry);
    // The actual_tag is shifted to the left by one and contains the marking
    // bit in the LSB. To ignore that during the type check, simply add one to
    // the (shifted) range.
    constexpr int kTagShift = internal::kCppHeapPointerTagShift;
    uint32_t first_tag = static_cast<uint32_t>(lower_bound) << kTagShift;
    uint32_t last_tag = (static_cast<uint32_t>(upper_bound) << kTagShift) + 1;
    return actual_tag >= first_tag && actual_tag <= last_tag;
  }
};

constexpr CppHeapPointerTagRange kAnyCppHeapPointer(
    CppHeapPointerTag::kFirstTag, CppHeapPointerTag::kLastTag);

namespace internal {

#ifdef V8_COMPRESS_POINTERS
V8_INLINE static Address* GetCppHeapPointerTableBase(v8::Isolate* isolate) {
  Address addr = reinterpret_cast<Address>(isolate) +
                 Internals::kIsolateCppHeapPointerTableOffset +
                 Internals::kExternalPointerTableBasePointerOffset;
  return *reinterpret_cast<Address**>(addr);
}
#endif  // V8_COMPRESS_POINTERS

template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound,
          typename T>
V8_INLINE static T* ReadCppHeapPointerField(v8::Isolate* isolate,
                                            Address heap_object_ptr,
                                            int offset) {
#ifdef V8_COMPRESS_POINTERS
  // See src/sandbox/cppheap-pointer-table-inl.h. Logic duplicated here so
  // it can be inlined and doesn't require an additional call.
  static_assert(lower_bound <= upper_bound);
  CppHeapPointerTagRange tag_range(lower_bound, upper_bound);

  const CppHeapPointerHandle handle =
      Internals::ReadRawField<CppHeapPointerHandle>(heap_object_ptr, offset);
  // TODO(saelo): can we remove this check since we should just fail the type
  // check for the null entry, in which case we can also just return nullptr?
  if (handle == 0) {
    return reinterpret_cast<T*>(kNullAddress);
  }
  const uint32_t index = handle >> kExternalPointerIndexShift;
  const Address* table = GetCppHeapPointerTableBase(isolate);
  const std::atomic<Address>* ptr =
      reinterpret_cast<const std::atomic<Address>*>(&table[index]);
  Address entry = std::atomic_load_explicit(ptr, std::memory_order_relaxed);

  Address pointer = entry;
  if (V8_LIKELY(tag_range.CheckTagOf(entry))) {
    pointer = entry >> kCppHeapPointerPayloadShift;
  }
#ifdef V8_TARGET_ARCH_ARM64
  // On Arm64, we potentially have top byte ignore, and so we cannot rely on a
  // pointer access crashing if some of the top 16 bits are set (only if the
  // second most significant byte is non-zero). In addition, there shouldn't be
  // a different on Arm64 between returning nullptr or the original entry, since
  // it will simply compile to a `csel x0, x8, xzr, lo` instead of a
  // `csel x0, x10, x8, lo` instruction.
  else {
    pointer = 0;
  }
#endif
  return reinterpret_cast<T*>(pointer);
#else   // !V8_COMPRESS_POINTERS
  return reinterpret_cast<T*>(
      Internals::ReadRawField<Address>(heap_object_ptr, offset));
#endif  // !V8_COMPRESS_POINTERS
}

template <CppHeapPointerTag tag, typename T>
V8_INLINE static T* ReadCppHeapPointerField(v8::Isolate* isolate,
                                            Address heap_object_ptr,
                                            int offset) {
  return ReadCppHeapPointerField<tag, tag, T>(isolate, heap_object_ptr, offset);
}

}  // namespace internal
}  // namespace v8

#endif  // INCLUDE_V8_SANDBOX_H_
