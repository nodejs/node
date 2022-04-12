// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_INL_H_
#define V8_SANDBOX_EXTERNAL_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/execution/isolate.h"
#include "src/sandbox/external-pointer-table-inl.h"
#include "src/sandbox/external-pointer.h"

namespace v8 {
namespace internal {

V8_INLINE Address DecodeExternalPointer(const Isolate* isolate,
                                        ExternalPointer_t encoded_pointer,
                                        ExternalPointerTag tag) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  STATIC_ASSERT(kExternalPointerSize == kInt32Size);
  uint32_t index = encoded_pointer >> kExternalPointerIndexShift;
  return isolate->external_pointer_table().Get(index, tag);
#else
  STATIC_ASSERT(kExternalPointerSize == kSystemPointerSize);
  return encoded_pointer;
#endif
}

V8_INLINE void InitExternalPointerField(Address field_address, Isolate* isolate,
                                        ExternalPointerTag tag) {
  InitExternalPointerField(field_address, isolate, kNullExternalPointer, tag);
}

V8_INLINE void InitExternalPointerField(Address field_address, Isolate* isolate,
                                        Address value, ExternalPointerTag tag) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  ExternalPointer_t index = isolate->external_pointer_table().Allocate();
  isolate->external_pointer_table().Set(index, value, tag);
  index <<= kExternalPointerIndexShift;
  base::Memory<ExternalPointer_t>(field_address) = index;
#else
  // Pointer compression causes types larger than kTaggedSize to be unaligned.
  constexpr bool v8_pointer_compression_unaligned =
      kExternalPointerSize > kTaggedSize;
  ExternalPointer_t encoded_value = static_cast<ExternalPointer_t>(value);
  if (v8_pointer_compression_unaligned) {
    base::WriteUnalignedValue<ExternalPointer_t>(field_address, encoded_value);
  } else {
    base::Memory<ExternalPointer_t>(field_address) = encoded_value;
  }
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
}

V8_INLINE ExternalPointer_t ReadRawExternalPointerField(Address field_address) {
  // Pointer compression causes types larger than kTaggedSize to be unaligned.
  constexpr bool v8_pointer_compression_unaligned =
      kExternalPointerSize > kTaggedSize;
  if (v8_pointer_compression_unaligned) {
    return base::ReadUnalignedValue<ExternalPointer_t>(field_address);
  } else {
    return base::Memory<ExternalPointer_t>(field_address);
  }
}

V8_INLINE Address ReadExternalPointerField(Address field_address,
                                           const Isolate* isolate,
                                           ExternalPointerTag tag) {
  ExternalPointer_t encoded_value = ReadRawExternalPointerField(field_address);
  return DecodeExternalPointer(isolate, encoded_value, tag);
}

V8_INLINE void WriteExternalPointerField(Address field_address,
                                         Isolate* isolate, Address value,
                                         ExternalPointerTag tag) {
#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
  ExternalPointer_t index = base::Memory<ExternalPointer_t>(field_address);
  index >>= kExternalPointerIndexShift;
  isolate->external_pointer_table().Set(index, value, tag);
#else
  // Pointer compression causes types larger than kTaggedSize to be unaligned.
  constexpr bool v8_pointer_compression_unaligned =
      kExternalPointerSize > kTaggedSize;
  ExternalPointer_t encoded_value = static_cast<ExternalPointer_t>(value);
  if (v8_pointer_compression_unaligned) {
    base::WriteUnalignedValue<ExternalPointer_t>(field_address, encoded_value);
  } else {
    base::Memory<ExternalPointer_t>(field_address) = encoded_value;
  }
#endif  // V8_SANDBOXED_EXTERNAL_POINTERS
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_EXTERNAL_POINTER_INL_H_
