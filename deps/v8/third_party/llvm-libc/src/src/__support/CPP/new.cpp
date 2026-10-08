//===-- Implementation of custom operator delete --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "new.h"
#include "hdr/func/free.h"

void operator delete(void *mem) noexcept { ::free(mem); }

void operator delete(void *mem, std::align_val_t) noexcept { ::free(mem); }

void operator delete(void *mem, size_t) noexcept { ::free(mem); }

void operator delete(void *mem, size_t, std::align_val_t) noexcept {
#ifdef LIBC_TARGET_OS_IS_WINDOWS
  ::_aligned_free(mem);
#else
  ::free(mem);
#endif
}

void operator delete[](void *mem) noexcept { ::free(mem); }

void operator delete[](void *mem, std::align_val_t) noexcept {
#ifdef LIBC_TARGET_OS_IS_WINDOWS
  ::_aligned_free(mem);
#else
  ::free(mem);
#endif
}

void operator delete[](void *mem, size_t) noexcept { ::free(mem); }

void operator delete[](void *mem, size_t, std::align_val_t) noexcept {
#ifdef LIBC_TARGET_OS_IS_WINDOWS
  ::_aligned_free(mem);
#else
  ::free(mem);
#endif
}
