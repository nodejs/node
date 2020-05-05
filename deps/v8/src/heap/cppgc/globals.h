// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_GLOBALS_H_
#define V8_HEAP_CPPGC_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

namespace cppgc {
namespace internal {

using Address = uint8_t*;
using ConstAddress = const uint8_t*;

// See 6.7.6 (http://eel.is/c++draft/basic.align) for alignment restrictions. We
// do not fully support all alignment restrictions (following
// alignof(std​::​max_­align_­t)) but limit to alignof(double).
//
// This means that any scalar type with stricter alignment requirements (in
// practice: long double) cannot be used unrestricted in garbage-collected
// objects.
//
// Note: We use the same allocation granularity on 32-bit and 64-bit systems.
constexpr size_t kAllocationGranularity = 8;
constexpr size_t kAllocationMask = kAllocationGranularity - 1;

constexpr size_t kPageSizeLog2 = 17;
constexpr size_t kPageSize = 1 << kPageSizeLog2;
constexpr size_t kPageOffsetMask = kPageSize - 1;
constexpr size_t kPageBaseMask = ~kPageOffsetMask;

constexpr size_t kLargeObjectSizeThreshold = kPageSize / 2;

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_GLOBALS_H_
