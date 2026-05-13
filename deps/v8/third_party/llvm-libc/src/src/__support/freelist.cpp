//===-- Implementation for freelist ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "freelist.h"

namespace LIBC_NAMESPACE_DECL {

void FreeList::push(Node *node) {
  if (begin_) {
    LIBC_ASSERT(Block::from_usable_space(node)->outer_size() ==
                    begin_->block()->outer_size() &&
                "freelist entries must have the same size");
    // Since the list is circular, insert the node immediately before begin_.
    node->prev = begin_->prev;
    node->next = begin_;
    begin_->prev->next = node;
    begin_->prev = node;
  } else {
    begin_ = node->prev = node->next = node;
  }
}

void FreeList::remove(Node *node) {
  LIBC_ASSERT(begin_ && "cannot remove from empty list");
  if (node == node->next) {
    LIBC_ASSERT(node == begin_ &&
                "a self-referential node must be the only element");
    begin_ = nullptr;
  } else {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    if (begin_ == node)
      begin_ = node->next;
  }
}

} // namespace LIBC_NAMESPACE_DECL
