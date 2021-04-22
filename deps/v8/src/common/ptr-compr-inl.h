// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_INL_H_
#define V8_COMMON_PTR_COMPR_INL_H_

#include "include/v8-internal.h"
#include "src/common/ptr-compr.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate-inl.h"

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS

IsolateRoot::IsolateRoot(const Isolate* isolate)
    : address_(isolate->isolate_root()) {}
IsolateRoot::IsolateRoot(const LocalIsolate* isolate)
    : address_(isolate->isolate_root()) {}

Address IsolateRoot::address() const {
  Address ret = address_;
  ret = reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(ret), kPtrComprIsolateRootAlignment));
  return ret;
}

// Compresses full-pointer representation of a tagged value to on-heap
// representation.
V8_INLINE Tagged_t CompressTagged(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

V8_INLINE constexpr Address GetIsolateRootAddress(Address on_heap_addr) {
  return RoundDown<kPtrComprIsolateRootAlignment>(on_heap_addr);
}

V8_INLINE Address GetIsolateRootAddress(IsolateRoot isolate) {
  return isolate.address();
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
  return GetIsolateRootAddress(on_heap_addr) + static_cast<Address>(raw_value);
}

// Decompresses any tagged value, preserving both weak- and smi- tags.
template <typename TOnHeapAddress>
V8_INLINE Address DecompressTaggedAny(TOnHeapAddress on_heap_addr,
                                      Tagged_t raw_value) {
  return DecompressTaggedPointer(on_heap_addr, raw_value);
}

STATIC_ASSERT(kPtrComprHeapReservationSize ==
              Internals::kPtrComprHeapReservationSize);
STATIC_ASSERT(kPtrComprIsolateRootAlignment ==
              Internals::kPtrComprIsolateRootAlignment);

#else

V8_INLINE Tagged_t CompressTagged(Address tagged) { UNREACHABLE(); }

V8_INLINE Address GetIsolateRootAddress(Address on_heap_addr) { UNREACHABLE(); }

V8_INLINE Address GetIsolateRootAddress(IsolateRoot isolate) { UNREACHABLE(); }

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

#endif  // V8_COMPRESS_POINTERS
}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_PTR_COMPR_INL_H_
