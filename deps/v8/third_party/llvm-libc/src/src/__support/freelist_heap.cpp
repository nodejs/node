//===-- Implementation for freelist_heap ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/freelist_heap.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

static LIBC_CONSTINIT FreeListHeap freelist_heap_symbols;
FreeListHeap *freelist_heap = &freelist_heap_symbols;

} // namespace LIBC_NAMESPACE_DECL
