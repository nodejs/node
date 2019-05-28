// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PTR_COMPR_INL_H_
#define V8_PTR_COMPR_INL_H_


#include "include/v8-internal.h"
#include "src/ptr-compr.h"

namespace v8 {
namespace internal {

#if V8_TARGET_ARCH_64_BIT
// Compresses full-pointer representation of a tagged value to on-heap
// representation.
V8_INLINE Tagged_t CompressTagged(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

enum class OnHeapAddressKind {
  kAnyOnHeapAddress,
  kIsolateRoot,
};

// Calculates isolate root value from any on-heap address.
template <OnHeapAddressKind kAddressKind = OnHeapAddressKind::kAnyOnHeapAddress>
V8_INLINE Address GetRootFromOnHeapAddress(Address on_heap_addr) {
  if (kAddressKind == OnHeapAddressKind::kIsolateRoot) return on_heap_addr;
  return RoundDown(on_heap_addr + kPtrComprIsolateRootBias,
                   kPtrComprIsolateRootAlignment);
}

// Decompresses weak or strong heap object pointer or forwarding pointer,
// preserving both weak- and smi- tags.
template <OnHeapAddressKind kAddressKind = OnHeapAddressKind::kAnyOnHeapAddress>
V8_INLINE Address DecompressTaggedPointer(Address on_heap_addr,
                                          Tagged_t raw_value) {
  // Current compression scheme requires |raw_value| to be sign-extended
  // from int32_t to intptr_t.
  intptr_t value = static_cast<intptr_t>(static_cast<int32_t>(raw_value));
  Address root = GetRootFromOnHeapAddress<kAddressKind>(on_heap_addr);
  return root + static_cast<Address>(value);
}

// Decompresses any tagged value, preserving both weak- and smi- tags.
template <OnHeapAddressKind kAddressKind = OnHeapAddressKind::kAnyOnHeapAddress>
V8_INLINE Address DecompressTaggedAny(Address on_heap_addr,
                                      Tagged_t raw_value) {
  // Current compression scheme requires |raw_value| to be sign-extended
  // from int32_t to intptr_t.
  intptr_t value = static_cast<intptr_t>(static_cast<int32_t>(raw_value));
  if (kUseBranchlessPtrDecompression) {
    // |root_mask| is 0 if the |value| was a smi or -1 otherwise.
    Address root_mask = static_cast<Address>(-(value & kSmiTagMask));
    Address root_or_zero =
        root_mask & GetRootFromOnHeapAddress<kAddressKind>(on_heap_addr);
    return root_or_zero + static_cast<Address>(value);
  } else {
    return HAS_SMI_TAG(value)
               ? static_cast<Address>(value)
               : (GetRootFromOnHeapAddress<kAddressKind>(on_heap_addr) +
                  static_cast<Address>(value));
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

#endif  // V8_TARGET_ARCH_64_BIT
}  // namespace internal
}  // namespace v8

#endif  // V8_PTR_COMPR_INL_H_
