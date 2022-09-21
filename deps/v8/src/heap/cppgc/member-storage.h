// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_MEMBER_STORAGE_H_
#define V8_HEAP_CPPGC_MEMBER_STORAGE_H_

#include "include/cppgc/internal/member-storage.h"

namespace cppgc {
namespace internal {

#if defined(CPPGC_POINTER_COMPRESSION)
class CageBaseGlobalUpdater final {
 public:
  CageBaseGlobalUpdater() = delete;
  static void UpdateCageBase(uintptr_t cage_base) {
    CPPGC_DCHECK(CageBaseGlobal::IsBaseConsistent());
    CPPGC_DCHECK(0u == (cage_base & CageBaseGlobal::kLowerHalfWordMask));
    CageBaseGlobal::g_base_ = cage_base | CageBaseGlobal::kLowerHalfWordMask;
  }

  static uintptr_t GetCageBase() {
    CPPGC_DCHECK(CageBaseGlobal::IsBaseConsistent());
    return CageBaseGlobal::g_base_ & ~CageBaseGlobal::kLowerHalfWordMask;
  }
};
#endif  // defined(CPPGC_POINTER_COMPRESSION)

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_MEMBER_STORAGE_H_
