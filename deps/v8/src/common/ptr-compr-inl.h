// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_INL_H_
#define V8_COMMON_PTR_COMPR_INL_H_

#include "include/v8-internal.h"
#include "src/common/ptr-compr.h"
#include "src/execution/isolate.h"

namespace v8 {
namespace internal {

#if V8_TARGET_ARCH_64_BIT
// Compresses full-pointer representation of a tagged value to on-heap
// representation.
V8_INLINE Tagged_t CompressTagged(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

// Calculates isolate root value from any on-heap address.
template <typename TOnHeapAddress>
V8_INLINE Address GetIsolateRoot(TOnHeapAddress on_heap_addr);

template <>
V8_INLINE Address GetIsolateRoot<Address>(Address on_heap_addr) {
  // We subtract 1 here in order to let the compiler generate addition of 32-bit
  // signed constant instead of 64-bit constant (the problem is that 2Gb looks
  // like a negative 32-bit value). It's correct because we will never use
  // leftmost address of V8 heap as |on_heap_addr|.
  return RoundDown<kPtrComprIsolateRootAlignment>(on_heap_addr +
                                                  kPtrComprIsolateRootBias - 1);
}

template <>
V8_INLINE Address GetIsolateRoot<Isolate*>(Isolate* isolate) {
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
  // Current compression scheme requires |raw_value| to be sign-extended
  // from int32_t to intptr_t.
  intptr_t value = static_cast<intptr_t>(static_cast<int32_t>(raw_value));
  Address root = GetIsolateRoot(on_heap_addr);
  return root + static_cast<Address>(value);
}

// Decompresses any tagged value, preserving both weak- and smi- tags.
template <typename TOnHeapAddress>
V8_INLINE Address DecompressTaggedAny(TOnHeapAddress on_heap_addr,
                                      Tagged_t raw_value) {
  if (kUseBranchlessPtrDecompressionInRuntime) {
    // Current compression scheme requires |raw_value| to be sign-extended
    // from int32_t to intptr_t.
    intptr_t value = static_cast<intptr_t>(static_cast<int32_t>(raw_value));
    // |root_mask| is 0 if the |value| was a smi or -1 otherwise.
    Address root_mask = static_cast<Address>(-(value & kSmiTagMask));
    Address root_or_zero = root_mask & GetIsolateRoot(on_heap_addr);
    return root_or_zero + static_cast<Address>(value);
  } else {
    return HAS_SMI_TAG(raw_value)
               ? DecompressTaggedSigned(raw_value)
               : DecompressTaggedPointer(on_heap_addr, raw_value);
  }
}

#ifdef V8_COMPRESS_POINTERS

STATIC_ASSERT(kPtrComprHeapReservationSize ==
              Internals::kPtrComprHeapReservationSize);
STATIC_ASSERT(kPtrComprIsolateRootBias == Internals::kPtrComprIsolateRootBias);
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
