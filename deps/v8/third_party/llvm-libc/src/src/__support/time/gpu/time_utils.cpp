//===-- Generic utilities for GPU timing ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "time_utils.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

#if defined(LIBC_TARGET_ARCH_IS_AMDGPU) || defined(__SPIRV__)
// This is expected to be initialized by the runtime if the default value is
// insufficient.
// TODO: Once we have another use-case for this we should put it in a common
// device environment struct.
gpu::Constant<uint64_t> __llvm_libc_clock_freq = clock_freq;
#endif

} // namespace LIBC_NAMESPACE_DECL
