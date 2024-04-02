// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_PTR_COMPR_H_
#define V8_COMMON_PTR_COMPR_H_

#include "src/base/memory.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

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

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_PTR_COMPR_H_
