// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_LOCAL_HANDLES_INL_H_
#define V8_HANDLES_LOCAL_HANDLES_INL_H_

#include "src/handles/local-handles.h"

#include "src/sanitizer/msan.h"

namespace v8 {
namespace internal {

// static
V8_INLINE Address* LocalHandleScope::GetHandle(LocalHeap* local_heap,
                                               Address value) {
  LocalHandles* handles = local_heap->handles();
  Address* result = handles->scope_.next;
  if (result == handles->scope_.limit) {
    result = handles->AddBlock();
  }
  DCHECK_LT(result, handles->scope_.limit);
  handles->scope_.next++;
  *result = value;
  return result;
}

LocalHandleScope::LocalHandleScope(LocalHeap* local_heap) {
  LocalHandles* handles = local_heap->handles();
  local_heap_ = local_heap;
  prev_next_ = handles->scope_.next;
  prev_limit_ = handles->scope_.limit;
  handles->scope_.level++;
}

LocalHandleScope::~LocalHandleScope() {
  LocalHandles* handles = local_heap_->handles();
  Address* old_limit = handles->scope_.limit;

  handles->scope_.next = prev_next_;
  handles->scope_.limit = prev_limit_;
  handles->scope_.level--;

  if (old_limit != handles->scope_.limit) {
    handles->RemoveBlocks();
    old_limit = handles->scope_.limit;
  }

  // TODO(dinfuehr): Zap handles

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(
      handles->scope_.next,
      static_cast<size_t>(reinterpret_cast<Address>(old_limit) -
                          reinterpret_cast<Address>(handles->scope_.next)));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_LOCAL_HANDLES_INL_H_
