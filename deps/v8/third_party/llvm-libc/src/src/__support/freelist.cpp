//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation for freelist.
///
//===----------------------------------------------------------------------===//

#include "freelist.h"

namespace LIBC_NAMESPACE_DECL {

void FreeList::push(Node *node) {
  if (begin_) {
    begin_->integrity_check();
    // Since the list is circular, insert the node immediately before begin_.
    node->prev_ = begin_->prev_;
    node->next_ = begin_;
    begin_->prev_->next_ = node;
    begin_->prev_ = node;
  } else {
    begin_ = node->prev_ = node->next_ = node;
  }
}

void FreeList::remove(Node *node) {
  LIBC_ASSERT(begin_ && "cannot remove from empty list");
  node->integrity_check();
  Node *next = node->next_;
  if (node == next) {
    LIBC_ASSERT(node == begin_ &&
                "a self-referential node must be the only element");
    begin_ = nullptr;
  } else {
    Node *prev = node->prev_;
    prev->next_ = next;
    next->prev_ = prev;
    if (begin_ == node)
      begin_ = next;
  }
}

void FreeList::integrity_check() const {
  if (!begin_)
    return;
  Node *curr = begin_;
  do {
    curr->integrity_check();
    curr = curr->next_;
  } while (curr != begin_);
}

} // namespace LIBC_NAMESPACE_DECL
