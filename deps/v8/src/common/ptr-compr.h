// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_H_
#define V8_COMMON_PTR_COMPR_H_

#include "src/base/memory.h"
#include "src/common/globals.h"

namespace v8::internal {

// This is just a collection of compression scheme related functions. Having
// such a class allows plugging different decompression scheme in certain
// places by introducing another CompressionScheme class with a customized
// implementation. This is useful, for example, for Code::code
// field (see CodeObjectSlot).
class V8HeapCompressionScheme {
 public:
  V8_INLINE static Address GetPtrComprCageBaseAddress(Address on_heap_addr);

  V8_INLINE static Address GetPtrComprCageBaseAddress(
      PtrComprCageBase cage_base);

  // Compresses full-pointer representation of a tagged value to on-heap
  // representation.
  // Must only be used for compressing object pointers since this function
  // assumes that we deal with a valid address inside the pointer compression
  // cage.
  V8_INLINE static Tagged_t CompressObject(Address tagged);
  // Compress a potentially invalid pointer.
  V8_INLINE static Tagged_t CompressAny(Address tagged);

  // Decompresses smi value.
  V8_INLINE static Address DecompressTaggedSigned(Tagged_t raw_value);

  // Decompresses any tagged value, preserving both weak- and smi- tags.
  template <typename TOnHeapAddress>
  V8_INLINE static Address DecompressTagged(TOnHeapAddress on_heap_addr,
                                            Tagged_t raw_value);

  // Given a 64bit raw value, found on the stack, calls the callback function
  // with all possible pointers that may be "contained" in compressed form in
  // this value, either as complete compressed pointers or as intermediate
  // (half-computed) results.
  template <typename ProcessPointerCallback>
  V8_INLINE static void ProcessIntermediatePointers(
      PtrComprCageBase cage_base, Address raw_value,
      ProcessPointerCallback callback);

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  // Process-wide cage base value used for decompression.
  V8_INLINE static void InitBase(Address base);
  V8_INLINE static Address base();

 private:
  static V8_EXPORT_PRIVATE uintptr_t base_ V8_CONSTINIT;
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE
};

#ifdef V8_EXTERNAL_CODE_SPACE

// Compression scheme used for fields containing InstructionStream objects
// (namely for the Code::code field). Same as
// V8HeapCompressionScheme but with a different base value.
class ExternalCodeCompressionScheme {
 public:
  V8_INLINE static Address PrepareCageBaseAddress(Address on_heap_addr);

  // Note that this compression scheme doesn't allow reconstruction of the cage
  // base value from any arbitrary value, thus the cage base has to be passed
  // explicitly to the decompression functions.
  static Address GetPtrComprCageBaseAddress(Address on_heap_addr) = delete;

  V8_INLINE static Address GetPtrComprCageBaseAddress(
      PtrComprCageBase cage_base);

  // Compresses full-pointer representation of a tagged value to on-heap
  // representation.
  // Must only be used for compressing object pointers (incl. SMI) since this
  // function assumes pointers to be inside the pointer compression cage.
  V8_INLINE static Tagged_t CompressObject(Address tagged);
  // Compress anything that does not follow the above requirements (e.g. a maybe
  // object, or a marker bit pattern).
  V8_INLINE static Tagged_t CompressAny(Address tagged);

  // Decompresses smi value.
  V8_INLINE static Address DecompressTaggedSigned(Tagged_t raw_value);

  // Decompresses any tagged value, preserving both weak- and smi- tags.
  template <typename TOnHeapAddress>
  V8_INLINE static Address DecompressTagged(TOnHeapAddress on_heap_addr,
                                            Tagged_t raw_value);

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
  // Process-wide cage base value used for decompression.
  V8_INLINE static void InitBase(Address base);
  V8_INLINE static Address base();

 private:
  static V8_EXPORT_PRIVATE uintptr_t base_ V8_CONSTINIT;
#endif  // V8_COMPRESS_POINTERS_IN_SHARED_CAGE
};

#endif  // V8_EXTERNAL_CODE_SPACE

// Accessors for fields that may be unaligned due to pointer compression.

template <typename V>
static inline V ReadMaybeUnalignedValue(Address p) {
  // Pointer compression causes types larger than kTaggedSize to be unaligned.
#ifdef V8_COMPRESS_POINTERS
  constexpr bool v8_pointer_compression_unaligned = sizeof(V) > kTaggedSize;
#else
  constexpr bool v8_pointer_compression_unaligned = false;
#endif
  // Bug(v8:8875) Double fields may be unaligned.
  constexpr bool unaligned_double_field =
      std::is_same<V, double>::value && kDoubleSize > kTaggedSize;
  if (unaligned_double_field || v8_pointer_compression_unaligned) {
    return base::ReadUnalignedValue<V>(p);
  } else {
    return base::Memory<V>(p);
  }
}

template <typename V>
static inline void WriteMaybeUnalignedValue(Address p, V value) {
  // Pointer compression causes types larger than kTaggedSize to be unaligned.
#ifdef V8_COMPRESS_POINTERS
  constexpr bool v8_pointer_compression_unaligned = sizeof(V) > kTaggedSize;
#else
  constexpr bool v8_pointer_compression_unaligned = false;
#endif
  // Bug(v8:8875) Double fields may be unaligned.
  constexpr bool unaligned_double_field =
      std::is_same<V, double>::value && kDoubleSize > kTaggedSize;
  if (unaligned_double_field || v8_pointer_compression_unaligned) {
    base::WriteUnalignedValue<V>(p, value);
  } else {
    base::Memory<V>(p) = value;
  }
}

}  // namespace v8::internal

#endif  // V8_COMMON_PTR_COMPR_H_
