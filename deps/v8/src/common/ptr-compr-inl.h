// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_INL_H_
#define V8_COMMON_PTR_COMPR_INL_H_

#include "include/v8-internal.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate-inl.h"

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS

PtrComprCageBase::PtrComprCageBase(const Isolate* isolate)
    : address_(isolate->cage_base()) {}
PtrComprCageBase::PtrComprCageBase(const LocalIsolate* isolate)
    : address_(isolate->cage_base()) {}

//
// V8HeapCompressionScheme
//

constexpr Address kPtrComprCageBaseMask = ~(kPtrComprCageBaseAlignment - 1);

// static
Address V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
    Address on_heap_addr) {
  return RoundDown<kPtrComprCageBaseAlignment>(on_heap_addr);
}

// static
Address V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
    PtrComprCageBase cage_base) {
  Address base = cage_base.address();
  V8_ASSUME((base & kPtrComprCageBaseMask) == base);
  base = reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(base), kPtrComprCageBaseAlignment));
  return base;
}

// static
void V8HeapCompressionScheme::InitBase(Address base) {
  CHECK_EQ(base, GetPtrComprCageBaseAddress(base));
#if defined(USING_V8_SHARED) && defined(V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE)
  set_base_non_inlined(base);
#else
  base_ = base;
#endif
}

// static
V8_CONST Address V8HeapCompressionScheme::base() {
#if defined(USING_V8_SHARED) && defined(V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE)
  Address base = base_non_inlined();
#else
  Address base = base_;
#endif
  // V8_ASSUME_ALIGNED is often not preserved across ptr-to-int casts (i.e. when
  // casting to an Address). To increase our chances we additionally encode the
  // same information in this V8_ASSUME.
  V8_ASSUME((base & kPtrComprCageBaseMask) == base);
  return reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(base), kPtrComprCageBaseAlignment));
}

// static
Tagged_t V8HeapCompressionScheme::CompressObject(Address tagged) {
  // This is used to help clang produce better code. Values which could be
  // invalid pointers need to be compressed with CompressAny.
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  V8_ASSUME((tagged & kPtrComprCageBaseMask) == base() || HAS_SMI_TAG(tagged));
#endif
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

// static
Tagged_t V8HeapCompressionScheme::CompressAny(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

// static
Address V8HeapCompressionScheme::DecompressTaggedSigned(Tagged_t raw_value) {
  // For runtime code the upper 32-bits of the Smi value do not matter.
  return static_cast<Address>(raw_value);
}

// static
template <typename TOnHeapAddress>
Address V8HeapCompressionScheme::DecompressTagged(TOnHeapAddress on_heap_addr,
                                                  Tagged_t raw_value) {
#ifdef V8_COMPRESS_POINTERS
  Address cage_base = base();
#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  DCHECK_WITH_MSG(cage_base != kNullAddress,
                  "V8HeapCompressionScheme::base is not initialized for "
                  "current thread");
#endif  // V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
#else
  Address cage_base = GetPtrComprCageBaseAddress(on_heap_addr);
#endif  // V8_COMPRESS_POINTERS
  Address result = cage_base + static_cast<Address>(raw_value);
  V8_ASSUME(static_cast<uint32_t>(result) == raw_value);
  return result;
}

// static
template <typename ProcessPointerCallback>
void V8HeapCompressionScheme::ProcessIntermediatePointers(
    PtrComprCageBase cage_base, Address raw_value,
    ProcessPointerCallback callback) {
  // If pointer compression is enabled, we may have random compressed pointers
  // on the stack that may be used for subsequent operations.
  // Extract, decompress and trace both halfwords.
  Address decompressed_low = V8HeapCompressionScheme::DecompressTagged(
      cage_base, static_cast<Tagged_t>(raw_value));
  callback(decompressed_low);
  Address decompressed_high = V8HeapCompressionScheme::DecompressTagged(
      cage_base,
      static_cast<Tagged_t>(raw_value >> (sizeof(Tagged_t) * CHAR_BIT)));
  callback(decompressed_high);
}

#ifdef V8_EXTERNAL_CODE_SPACE

//
// ExternalCodeCompressionScheme
//

// static
Address ExternalCodeCompressionScheme::PrepareCageBaseAddress(
    Address on_heap_addr) {
  return RoundDown<kPtrComprCageBaseAlignment>(on_heap_addr);
}

// static
Address ExternalCodeCompressionScheme::GetPtrComprCageBaseAddress(
    PtrComprCageBase cage_base) {
  Address base = cage_base.address();
  V8_ASSUME((base & kPtrComprCageBaseMask) == base);
  base = reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(base), kPtrComprCageBaseAlignment));
  return base;
}

// static
void ExternalCodeCompressionScheme::InitBase(Address base) {
  CHECK_EQ(base, PrepareCageBaseAddress(base));
#if defined(USING_V8_SHARED) && defined(V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE)
  set_base_non_inlined(base);
#else
  base_ = base;
#endif
}

// static
V8_CONST Address ExternalCodeCompressionScheme::base() {
#if defined(USING_V8_SHARED) && defined(V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE)
  Address base = base_non_inlined();
#else
  Address base = base_;
#endif
  // V8_ASSUME_ALIGNED is often not preserved across ptr-to-int casts (i.e. when
  // casting to an Address). To increase our chances we additionally encode the
  // same information in this V8_ASSUME.
  V8_ASSUME((base & kPtrComprCageBaseMask) == base);
  return reinterpret_cast<Address>(V8_ASSUME_ALIGNED(
      reinterpret_cast<void*>(base), kPtrComprCageBaseAlignment));
}

// static
Tagged_t ExternalCodeCompressionScheme::CompressObject(Address tagged) {
  // This is used to help clang produce better code. Values which could be
  // invalid pointers need to be compressed with CompressAny.
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  V8_ASSUME((tagged & kPtrComprCageBaseMask) == base() || HAS_SMI_TAG(tagged));
#endif
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

// static
Tagged_t ExternalCodeCompressionScheme::CompressAny(Address tagged) {
  return static_cast<Tagged_t>(static_cast<uint32_t>(tagged));
}

// static
Address ExternalCodeCompressionScheme::DecompressTaggedSigned(
    Tagged_t raw_value) {
  // For runtime code the upper 32-bits of the Smi value do not matter.
  return static_cast<Address>(raw_value);
}

// static
template <typename TOnHeapAddress>
Address ExternalCodeCompressionScheme::DecompressTagged(
    TOnHeapAddress on_heap_addr, Tagged_t raw_value) {
#ifdef V8_COMPRESS_POINTERS
  Address cage_base = base();
#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  DCHECK_WITH_MSG(cage_base != kNullAddress,
                  "ExternalCodeCompressionScheme::base is not initialized for "
                  "current thread");
#endif  // V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
#else
  Address cage_base = GetPtrComprCageBaseAddress(on_heap_addr);
#endif  // V8_COMPRESS_POINTERS
  Address result = cage_base + static_cast<Address>(raw_value);
  V8_ASSUME(static_cast<uint32_t>(result) == raw_value);
  return result;
}

#endif  // V8_EXTERNAL_CODE_SPACE

//
// Misc functions.
//

V8_INLINE PtrComprCageBase
GetPtrComprCageBaseFromOnHeapAddress(Address address) {
  return PtrComprCageBase(
      V8HeapCompressionScheme::GetPtrComprCageBaseAddress(address));
}

// Load the main pointer compression cage base.
V8_INLINE PtrComprCageBase GetPtrComprCageBase() {
  return PtrComprCageBase(V8HeapCompressionScheme::base());
}

#else

//
// V8HeapCompressionScheme
//

// static
Address V8HeapCompressionScheme::GetPtrComprCageBaseAddress(
    Address on_heap_addr) {
  UNREACHABLE();
}

// static
Tagged_t V8HeapCompressionScheme::CompressObject(Address tagged) {
  UNREACHABLE();
}

// static
Tagged_t V8HeapCompressionScheme::CompressAny(Address tagged) { UNREACHABLE(); }

// static
Address V8HeapCompressionScheme::DecompressTaggedSigned(Tagged_t raw_value) {
  UNREACHABLE();
}

// static
template <typename TOnHeapAddress>
Address V8HeapCompressionScheme::DecompressTagged(TOnHeapAddress on_heap_addr,
                                                  Tagged_t raw_value) {
  UNREACHABLE();
}

// static
template <typename ProcessPointerCallback>
void V8HeapCompressionScheme::ProcessIntermediatePointers(
    PtrComprCageBase cage_base, Address raw_value,
    ProcessPointerCallback callback) {
  UNREACHABLE();
}

//
// Misc functions.
//

V8_INLINE constexpr PtrComprCageBase GetPtrComprCageBaseFromOnHeapAddress(
    Address address) {
  return PtrComprCageBase();
}

V8_INLINE PtrComprCageBase GetPtrComprCageBase() { return PtrComprCageBase(); }

#endif  // V8_COMPRESS_POINTERS

V8_INLINE PtrComprCageBase GetPtrComprCageBase(Tagged<HeapObject> object) {
  return GetPtrComprCageBaseFromOnHeapAddress(object.ptr());
}

#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE

PtrComprCageAccessScope::PtrComprCageAccessScope(Isolate* isolate)
    : cage_base_(V8HeapCompressionScheme::base()) {
  V8HeapCompressionScheme::InitBase(isolate->cage_base());
}

PtrComprCageAccessScope::~PtrComprCageAccessScope() {
  V8HeapCompressionScheme::InitBase(cage_base_);
}

#endif  // V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_PTR_COMPR_INL_H_
