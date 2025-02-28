// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ZAPPING_H_
#define V8_HEAP_ZAPPING_H_

#include <cstdint>

#include "include/v8-internal.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"

namespace v8::internal::heap {

// Zapping is needed for verify heap, and always done in debug builds.
inline bool ShouldZapGarbage() {
#ifdef DEBUG
  return true;
#else
#ifdef VERIFY_HEAP
  return v8_flags.verify_heap;
#else
  return false;
#endif
#endif
}

inline uintptr_t ZapValue() {
  return v8_flags.clear_free_memory ? kClearedFreeMemoryValue : kZapValue;
}

// Zaps a contiguous block of regular memory [start..(start+size_in_bytes)[ with
// a given zap value.
void ZapBlock(Address start, size_t size_in_bytes, uintptr_t zap_value);

// Zaps a contiguous block of code memory [start..(start+size_in_bytes)[ with
// kCodeZapValue.
V8_EXPORT_PRIVATE void ZapCodeBlock(Address start, int size_in_bytes);

}  // namespace v8::internal::heap

#endif  // V8_HEAP_ZAPPING_H_
