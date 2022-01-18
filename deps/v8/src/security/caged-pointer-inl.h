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

#ifdef V8_CAGED_POINTERS

V8_INLINE CagedPointer_t ReadCagedPointerField(Address field_address,
                                               PtrComprCageBase cage_base) {
  // Caged pointers are currently only used if the sandbox is enabled.
  DCHECK(V8_HEAP_SANDBOX_BOOL);

  Address caged_pointer = base::ReadUnalignedValue<Address>(field_address);

  Address offset = caged_pointer >> kCagedPointerShift;
  Address pointer = cage_base.address() + offset;
  return pointer;
}

V8_INLINE void WriteCagedPointerField(Address field_address,
                                      PtrComprCageBase cage_base,
                                      CagedPointer_t pointer) {
  // Caged pointers are currently only used if the sandbox is enabled.
  DCHECK(V8_HEAP_SANDBOX_BOOL);

  // The pointer must point into the virtual memory cage.
  DCHECK(GetProcessWideVirtualMemoryCage()->Contains(pointer));

  Address offset = pointer - cage_base.address();
  Address caged_pointer = offset << kCagedPointerShift;
  base::WriteUnalignedValue<Address>(field_address, caged_pointer);
}

#endif  // V8_CAGED_POINTERS

}  // namespace internal
}  // namespace v8

#endif  // V8_SECURITY_CAGED_POINTER_INL_H_
