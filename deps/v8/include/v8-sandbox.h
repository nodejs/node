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
 */
enum class CppHeapPointerTag : uint64_t {
  kDefaultTag = internal::ExternalPointerTag::kExternalObjectValueTag,
};

namespace internal {

#ifdef V8_COMPRESS_POINTERS
V8_INLINE static Address* GetCppHeapPointerTableBase(v8::Isolate* isolate) {
  Address addr = reinterpret_cast<Address>(isolate) +
                 Internals::kIsolateCppHeapPointerTableOffset +
                 Internals::kExternalPointerTableBasePointerOffset;
  return *reinterpret_cast<Address**>(addr);
}
#endif  // V8_COMPRESS_POINTERS

template <CppHeapPointerTag tag, typename T>
V8_INLINE static T* ReadCppHeapPointerField(v8::Isolate* isolate,
                                            Address heap_object_ptr,
                                            int offset) {
#ifdef V8_COMPRESS_POINTERS
  static_assert(tag != static_cast<CppHeapPointerTag>(kExternalPointerNullTag));
  // See src/sandbox/external-pointer-table-inl.h. Logic duplicated here so
  // it can be inlined and doesn't require an additional call.
  const CppHeapPointerHandle handle =
      Internals::ReadRawField<CppHeapPointerHandle>(heap_object_ptr, offset);
  if (handle == 0) {
    return reinterpret_cast<T*>(kNullAddress);
  }
  const uint32_t index = handle >> kExternalPointerIndexShift;
  const Address* table = GetCppHeapPointerTableBase(isolate);
  const std::atomic<Address>* ptr =
      reinterpret_cast<const std::atomic<Address>*>(&table[index]);
  Address entry = std::atomic_load_explicit(ptr, std::memory_order_relaxed);
  return reinterpret_cast<T*>(entry & ~static_cast<uint64_t>(tag));
#else   // !V8_COMPRESS_POINTERS
  return reinterpret_cast<T*>(
      Internals::ReadRawField<Address>(heap_object_ptr, offset));
#endif  // !V8_COMPRESS_POINTERS
}

}  // namespace internal
}  // namespace v8

#endif  // INCLUDE_V8_SANDBOX_H_
