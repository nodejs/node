// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_INL_H_
#define V8_COMMON_PTR_COMPR_INL_H_

#include "include/v8-internal.h"
#include "src/common/ptr-compr.h"
#include "src/execution/isolate.h"
#include "src/execution/off-thread-isolate-inl.h"

namespace v8 {
namespace internal {

#if V8_TARGET_ARCH_64_BIT
// Compresses full-pointer representation of a tagged value to on-heap
// representation.
V8_INLINE Tagged_t CompressTagged(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

V8_INLINE Address GetIsolateRoot(Address on_heap_addr) {
  // We subtract 1 here in order to let the compiler generate addition of 32-bit
  // signed constant instead of 64-bit constant (the problem is that 2Gb looks
  // like a negative 32-bit value). It's correct because we will never use
  // leftmost address of V8 heap as |on_heap_addr|.
  return RoundDown<kPtrComprIsolateRootAlignment>(on_heap_addr);
}

V8_INLINE Address GetIsolateRoot(const Isolate* isolate) {
  Address isolate_root = isolate->isolate_root();
#ifdef V8_COMPRESS_POINTERS
  isolate_root = reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(isolate_root), kPtrComprIsolateRootAlignment));
#endif
  return isolate_root;
}

V8_INLINE Address GetIsolateRoot(const OffThreadIsolate* isolate) {
  Address isolate_root = isolate->isolate_root();
#ifdef V8_COMPRESS_POINTERS
  isolate_root = reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(isolate_root), kPtrComprIsolateRootAlignment));
#endif
  return isolate_root;
}

// Decompresses smi value.
V8_INLINE Address DecompressTaggedSigned(Tagged_t raw_value) {
  // For runtime code the upper 32-bits of the Smi value do not matter.
  return static_cast<Address>(raw_value);
}

// Decompresses weak or strong heap object pointer or forwarding pointer,
// preserving both weak- and smi- tags.
template <typename TOnHeapAddress>
V8_INLINE Address DecompressTaggedPointer(TOnHeapAddress on_heap_addr,
                                          Tagged_t raw_value) {
  return GetIsolateRoot(on_heap_addr) + static_cast<Address>(raw_value);
}

// Decompresses any tagged value, preserving both weak- and smi- tags.
template <typename TOnHeapAddress>
V8_INLINE Address DecompressTaggedAny(TOnHeapAddress on_heap_addr,
                                      Tagged_t raw_value) {
  return DecompressTaggedPointer(on_heap_addr, raw_value);
}

#ifdef V8_COMPRESS_POINTERS

STATIC_ASSERT(kPtrComprHeapReservationSize ==
              Internals::kPtrComprHeapReservationSize);
STATIC_ASSERT(kPtrComprIsolateRootAlignment ==
              Internals::kPtrComprIsolateRootAlignment);

#endif  // V8_COMPRESS_POINTERS

#else

V8_INLINE Tagged_t CompressTagged(Address tagged) { UNREACHABLE(); }

V8_INLINE Address DecompressTaggedSigned(Tagged_t raw_value) { UNREACHABLE(); }

template <typename TOnHeapAddress>
V8_INLINE Address DecompressTaggedPointer(TOnHeapAddress on_heap_addr,
                                          Tagged_t raw_value) {
  UNREACHABLE();
}

template <typename TOnHeapAddress>
V8_INLINE Address DecompressTaggedAny(TOnHeapAddress on_heap_addr,
                                      Tagged_t raw_value) {
  UNREACHABLE();
}

#endif  // V8_TARGET_ARCH_64_BIT
}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_PTR_COMPR_INL_H_
