// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#include "handles.h"

namespace v8 {
namespace internal {


int HandleScope::NumberOfHandles(Isolate* isolate) {
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  int n = impl->blocks()->length();
  if (n == 0) return 0;
  return ((n - 1) * kHandleBlockSize) + static_cast<int>(
      (isolate->handle_scope_data()->next - impl->blocks()->last()));
}


Object** HandleScope::Extend(Isolate* isolate) {
  HandleScopeData* current = isolate->handle_scope_data();

  Object** result = current->next;

  ASSERT(result == current->limit);
  // Make sure there's at least one scope on the stack and that the
  // top of the scope stack isn't a barrier.
  if (!Utils::ApiCheck(current->level != 0,
                       "v8::HandleScope::CreateHandle()",
                       "Cannot create a handle without a HandleScope")) {
    return NULL;
  }
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  // If there's more room in the last block, we use that. This is used
  // for fast creation of scopes after scope barriers.
  if (!impl->blocks()->is_empty()) {
    Object** limit = &impl->blocks()->last()[kHandleBlockSize];
    if (current->limit != limit) {
      current->limit = limit;
      ASSERT(limit - current->next < kHandleBlockSize);
    }
  }

  // If we still haven't found a slot for the handle, we extend the
  // current handle scope by allocating a new handle block.
  if (result == current->limit) {
    // If there's a spare block, use it for growing the current scope.
    result = impl->GetSpareOrNewBlock();
    // Add the extension to the global list of blocks, but count the
    // extension as part of the current scope.
    impl->blocks()->Add(result);
    current->limit = &result[kHandleBlockSize];
  }

  return result;
}


void HandleScope::DeleteExtensions(Isolate* isolate) {
  HandleScopeData* current = isolate->handle_scope_data();
  isolate->handle_scope_implementer()->DeleteExtensions(current->limit);
}


#ifdef ENABLE_HANDLE_ZAPPING
void HandleScope::ZapRange(Object** start, Object** end) {
  ASSERT(end - start <= kHandleBlockSize);
  for (Object** p = start; p != end; p++) {
    *reinterpret_cast<Address*>(p) = v8::internal::kHandleZapValue;
  }
}
#endif


Address HandleScope::current_level_address(Isolate* isolate) {
  return reinterpret_cast<Address>(&isolate->handle_scope_data()->level);
}


Address HandleScope::current_next_address(Isolate* isolate) {
  return reinterpret_cast<Address>(&isolate->handle_scope_data()->next);
}


Address HandleScope::current_limit_address(Isolate* isolate) {
  return reinterpret_cast<Address>(&isolate->handle_scope_data()->limit);
}


DeferredHandleScope::DeferredHandleScope(Isolate* isolate)
    : impl_(isolate->handle_scope_implementer()) {
  impl_->BeginDeferredScope();
  HandleScopeData* data = impl_->isolate()->handle_scope_data();
  Object** new_next = impl_->GetSpareOrNewBlock();
  Object** new_limit = &new_next[kHandleBlockSize];
  ASSERT(data->limit == &impl_->blocks()->last()[kHandleBlockSize]);
  impl_->blocks()->Add(new_next);

#ifdef DEBUG
  prev_level_ = data->level;
#endif
  data->level++;
  prev_limit_ = data->limit;
  prev_next_ = data->next;
  data->next = new_next;
  data->limit = new_limit;
}


DeferredHandleScope::~DeferredHandleScope() {
  impl_->isolate()->handle_scope_data()->level--;
  ASSERT(handles_detached_);
  ASSERT(impl_->isolate()->handle_scope_data()->level == prev_level_);
}


DeferredHandles* DeferredHandleScope::Detach() {
  DeferredHandles* deferred = impl_->Detach(prev_limit_);
  HandleScopeData* data = impl_->isolate()->handle_scope_data();
  data->next = prev_next_;
  data->limit = prev_limit_;
#ifdef DEBUG
  handles_detached_ = true;
#endif
  return deferred;
}

} }  // namespace v8::internal
