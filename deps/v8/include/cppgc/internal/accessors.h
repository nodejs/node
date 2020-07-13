// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_ACCESSORS_H_
#define INCLUDE_CPPGC_INTERNAL_ACCESSORS_H_

#include "cppgc/internal/api-constants.h"

namespace cppgc {

class Heap;

namespace internal {

inline cppgc::Heap* GetHeapFromPayload(const void* payload) {
  return *reinterpret_cast<cppgc::Heap**>(
      ((reinterpret_cast<uintptr_t>(payload) & api_constants::kPageBaseMask) +
       api_constants::kGuardPageSize) +
      api_constants::kHeapOffset);
}

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_ACCESSORS_H_
