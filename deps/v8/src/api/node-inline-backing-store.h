// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_NODE_INLINE_BACKING_STORE_H_
#define V8_API_NODE_INLINE_BACKING_STORE_H_

#include "../../include/v8-array-buffer.h"

#if defined(__clang__)
#if __has_feature(address_sanitizer) || __has_feature(hwaddress_sanitizer)
#define V8_NODE_INSTRUMENTED_NEW
#endif
#endif
#if !defined(DEBUG) && !defined(_DEBUG) && !defined(V8_ENABLE_SANDBOX) && \
    !defined(V8_USE_ADDRESS_SANITIZER) && !defined(V8_USE_HWADDRESS_SANITIZER) && \
    !defined(__SANITIZE_ADDRESS__) && !defined(LEAK_SANITIZER) && \
    !defined(V8_NODE_INSTRUMENTED_NEW) && defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
#define V8_NODE_CAN_INLINE_BACKING_STORE
#endif
#undef V8_NODE_INSTRUMENTED_NEW

namespace v8::internal {

class
#if V8_HAS_ATTRIBUTE_VISIBILITY && !defined(V8_OS_WIN)
    __attribute__((visibility("hidden")))
#endif
    NodeInlineBackingStore {
 public:
  using Account =
      void (*)(v8::ArrayBuffer::Allocator*, void*, size_t, bool) noexcept;

  static bool IsSupported();
  static bool NewMany(v8::Isolate* isolate, size_t length, Account account,
                      v8::BackingStore::DeleterCallback raw_release,
                      std::unique_ptr<v8::BackingStore>* slots, size_t count);
};

}  // namespace v8::internal

#endif  // V8_API_NODE_INLINE_BACKING_STORE_H_
