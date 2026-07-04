//===-- Intrusive queue implementation. -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// An intrusive list that implements the insque and remque semantics.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_INTRUSIVE_LIST_H
#define LLVM_LIBC_SRC___SUPPORT_INTRUSIVE_LIST_H

#include "common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

class IntrusiveList {
  struct IntrusiveNodeHeader {
    IntrusiveNodeHeader *next;
    IntrusiveNodeHeader *prev;
  };

public:
  LIBC_INLINE static void insert(void *elem, void *prev) {
    auto elem_header = static_cast<IntrusiveNodeHeader *>(elem);
    auto prev_header = static_cast<IntrusiveNodeHeader *>(prev);

    if (!prev_header) {
      // The list is linear and elem will be the only element.
      elem_header->next = nullptr;
      elem_header->prev = nullptr;
      return;
    }

    auto next = prev_header->next;

    elem_header->next = next;
    elem_header->prev = prev_header;

    prev_header->next = elem_header;
    if (next)
      next->prev = elem_header;
  }

  LIBC_INLINE static void remove(void *elem) {
    auto elem_header = static_cast<IntrusiveNodeHeader *>(elem);

    auto prev = elem_header->prev;
    auto next = elem_header->next;

    if (prev)
      prev->next = next;
    if (next)
      next->prev = prev;
  }
};

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_INTRUSIVE_LIST_H
