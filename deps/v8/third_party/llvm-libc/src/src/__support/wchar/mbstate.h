//===-- Definition of mbstate-----------------------------------*-- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MBSTATE_H
#define LLVM_LIBC_SRC___SUPPORT_MBSTATE_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/char32_t.h"
#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

struct mbstate {
  // store a partial codepoint (in UTF-32)
  char32_t partial = 0;

  /*
  Progress towards a conversion
    Increases with each push(...) until it reaches total_bytes
    Decreases with each pop(...) until it reaches 0
  */
  uint8_t bytes_stored = 0;

  // Total number of bytes that will be needed to represent this character
  uint8_t total_bytes = 0;
};

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MBSTATE_H
