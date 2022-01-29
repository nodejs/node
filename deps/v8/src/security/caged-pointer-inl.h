// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SECURITY_CAGED_POINTER_INL_H_
#define V8_SECURITY_CAGED_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/execution/isolate.h"
#include "src/security/caged-pointer.h"

namespace v8 {
namespace internal {

V8_INLINE Address ReadCagedPointerField(Address field_address,
                                        PtrComprCageBase cage_base) {
#ifdef V8_CAGED_POINTERS
  // Caged pointers are currently only used if the sandbox is enabled.
  DCHECK(V8_HEAP_SANDBOX_BOOL);

  CagedPointer_t caged_pointer =
      base::ReadUnalignedValue<CagedPointer_t>(field_address);

  Address offset = caged_pointer >> kCagedPointerShift;
  Address pointer = cage_base.address() + offset;
  return pointer;
#else
  return base::ReadUnalignedValue<Address>(field_address);
#endif
}

V8_INLINE void WriteCagedPointerField(Address field_address,
                                      PtrComprCageBase cage_base,
                                      Address pointer) {
#ifdef V8_CAGED_POINTERS
  // Caged pointers are currently only used if the sandbox is enabled.
  DCHECK(V8_HEAP_SANDBOX_BOOL);

  // The pointer must point into the virtual memory cage.
  DCHECK(GetProcessWideVirtualMemoryCage()->Contains(pointer));

  Address offset = pointer - cage_base.address();
  CagedPointer_t caged_pointer = offset << kCagedPointerShift;
  base::WriteUnalignedValue<CagedPointer_t>(field_address, caged_pointer);
#else
  base::WriteUnalignedValue<Address>(field_address, pointer);
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SECURITY_CAGED_POINTER_INL_H_
