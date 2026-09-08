//===-- GPU memory allocator implementation ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_GPU_ALLOCATOR_H
#define LLVM_LIBC_SRC___SUPPORT_GPU_ALLOCATOR_H

#include "hdr/stdint_proxy.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace gpu {

void *allocate(uint64_t size);
void deallocate(void *ptr);
void *reallocate(void *ptr, uint64_t size);
void *aligned_allocate(uint32_t alignment, uint64_t size);

} // namespace gpu
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_GPU_ALLOCATOR_H
