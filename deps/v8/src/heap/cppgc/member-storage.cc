// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/member-storage.h"

namespace cppgc {
namespace internal {

#if defined(CPPGC_POINTER_COMPRESSION)
uintptr_t CageBaseGlobal::g_base_ = CageBaseGlobal::kLowerHalfWordMask;
#endif  // defined(CPPGC_POINTER_COMPRESSION)

}  // namespace internal
}  // namespace cppgc
