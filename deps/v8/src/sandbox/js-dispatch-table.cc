// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/js-dispatch-table.h"

#include "src/common/code-memory-access-inl.h"
#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/objects/code-inl.h"
#include "src/sandbox/js-dispatch-table-inl.h"


namespace v8 {
namespace internal {

void JSDispatchEntry::CheckFieldOffsets() {
  static_assert(JSDispatchEntry::kEntrypointOffset ==
                offsetof(JSDispatchEntry, entrypoint_));
  static_assert(JSDispatchEntry::kCodeObjectOffset ==
                offsetof(JSDispatchEntry, encoded_word_));
#if defined(V8_TARGET_ARCH_64_BIT)
#ifdef V8_TARGET_BIG_ENDIAN
  // 2-byte parameter count is on the least significant side of encoded_word_.
  constexpr int kBigEndianParamCountOffset = sizeof(Address) - sizeof(uint16_t);
  static_assert(sizeof(encoded_word_) == sizeof(Address));
  static_assert(JSDispatchEntry::kParameterCountOffset ==
                offsetof(JSDispatchEntry, encoded_word_) +
                    kBigEndianParamCountOffset);
#else
  static_assert(JSDispatchEntry::kParameterCountOffset ==
                offsetof(JSDispatchEntry, encoded_word_));
#endif  // V8_TARGET_BIG_ENDIAN
  static_assert(kParameterCountMask == 0xffff);
  static_assert(kParameterCountSize == 2);
#elif defined(V8_TARGET_ARCH_32_BIT)
  static_assert(JSDispatchEntry::kParameterCountOffset ==
                offsetof(JSDispatchEntry, parameter_count_));
  static_assert(kParameterCountSize ==
                sizeof(JSDispatchEntry::parameter_count_));
#else
#error "Unsupported Architecture"
#endif
}

void JSDispatchTable::PrintEntry(JSDispatchHandle handle) {
  uint32_t index = HandleToIndex(handle);
  i::PrintF("JSDispatchEntry (handle: %u) @ %p\n", handle.value(), &at(index));
  i::PrintF("* code %p\n", reinterpret_cast<void*>(GetCode(handle).address()));
  i::PrintF("* params %d\n", at(HandleToIndex(handle)).GetParameterCount());
  i::PrintF("* entrypoint %p\n",
            reinterpret_cast<void*>(GetEntrypoint(handle)));
}

void JSDispatchTable::PrintCurrentTieringRequest(JSDispatchHandle handle,
                                                 Isolate* isolate,
                                                 std::ostream& os) {
#define CASE(name, ...)                                               \
  if (IsTieringRequested(handle, TieringBuiltin::k##name, isolate)) { \
    os << #name;                                                      \
    return;                                                           \
  }
  BUILTIN_LIST_BASE_TIERING(CASE)
#undef CASE
}

}  // namespace internal
}  // namespace v8
