// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_PAGE_INL_H_
#define V8_HEAP_CPPGC_HEAP_PAGE_INL_H_

#include "src/heap/cppgc/heap-page.h"

namespace cppgc {
namespace internal {

// static
BasePage* BasePage::FromPayload(void* payload) {
  return reinterpret_cast<BasePage*>(
      (reinterpret_cast<uintptr_t>(payload) & kPageBaseMask) + kGuardPageSize);
}

// static
const BasePage* BasePage::FromPayload(const void* payload) {
  return reinterpret_cast<const BasePage*>(
      (reinterpret_cast<uintptr_t>(const_cast<void*>(payload)) &
       kPageBaseMask) +
      kGuardPageSize);
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_PAGE_INL_H_
