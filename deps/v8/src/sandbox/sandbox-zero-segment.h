// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_SANDBOX_ZERO_SEGMENT_H_
#define V8_SANDBOX_SANDBOX_ZERO_SEGMENT_H_

#ifdef V8_ENABLE_SANDBOX

#include <algorithm>

#include "include/v8config.h"
#include "src/common/globals.h"
#include "src/objects/field-index.h"
#include "src/objects/js-objects.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8::internal {

// The first 4GB of the process' address space are reserved by the
// platform/allocator as an inaccessible or read-only zero segment to mitigate
// Smi<->HeapObject (and Wasm i31ref<->WasmStruct/WasmCustomMap) confusion bugs
// in which a 32-bit value is treated as a pointer and dereferenced.
inline constexpr size_t kZeroSegmentBaseSize = 4UL * GB;

// Smi<->HeapObject confusion in JS can lead to property accesses at an offset
// from a Smi value near 4GB:
// - Regular in-object properties are bounded by JSObject::kMaxInstanceSize.
// - Corrupted PropertyDetails/FieldIndex metadata can encode offsets up to
//   FieldIndex::kMaxOffset (https://crbug.com/574732380).
inline constexpr size_t kJSMaxInObjectPropertyOffset =
    JSObject::kMaxInstanceSize;
inline constexpr size_t kJSMaxCorruptedPropertyOffset = FieldIndex::kMaxOffset;
inline constexpr size_t kJSZeroSegmentPadding =
    std::max(kJSMaxInObjectPropertyOffset, kJSMaxCorruptedPropertyOffset);

#if V8_ENABLE_WEBASSEMBLY
// i31ref<->WasmStruct/WasmCustomMap confusion in Wasm can lead to struct
// field accesses up to WasmCustomMap::kMaxInstanceSize
// (https://crbug.com/558640961).
inline constexpr size_t kWasmZeroSegmentPadding =
    std::max(WasmStruct::kMaxInstanceSize, WasmCustomMap::kMaxInstanceSize);
#else
inline constexpr size_t kWasmZeroSegmentPadding = 0;
#endif  // V8_ENABLE_WEBASSEMBLY

inline constexpr size_t kZeroSegmentPadding =
    std::max(kJSZeroSegmentPadding, kWasmZeroSegmentPadding);

inline constexpr size_t kMinZeroSegmentSize =
    kZeroSegmentBaseSize + kZeroSegmentPadding;

// PartitionAlloc reserves 4GB + 1MB by default
// (user_space_zero_segment_size_mb = 4097).
static_assert(kZeroSegmentPadding <= 1 * MB);
static_assert(kMinZeroSegmentSize <= size_t{4097} * MB);

}  // namespace v8::internal

#endif  // V8_ENABLE_SANDBOX

#endif  // V8_SANDBOX_SANDBOX_ZERO_SEGMENT_H_
