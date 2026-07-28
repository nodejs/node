// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_HANDLE_SCOPE_IMPLEMENTER_INL_H_
#define V8_HANDLES_HANDLE_SCOPE_IMPLEMENTER_INL_H_

#include "src/handles/handle-scope-implementer.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

void HandleScopeImplementer::SaveContext(Tagged<Context> context) {
  saved_contexts_.push_back(context);
}

Tagged<Context> HandleScopeImplementer::RestoreContext() {
  Tagged<Context> last_context = saved_contexts_.back();
  saved_contexts_.pop_back();
  return last_context;
}

bool HandleScopeImplementer::HasSavedContexts() {
  return !saved_contexts_.empty();
}

void HandleScopeImplementer::LeaveContext() {
  DCHECK(!entered_contexts_.empty());
  entered_contexts_.pop_back();
  Isolate::FromHandleScopeImplementer(this)->set_last_entered_context(
      entered_contexts_.empty() ? Tagged<NativeContext>()
                                : entered_contexts_.back());
}

HandleScopeImplementer::EnteredContextRewindScope::EnteredContextRewindScope(
    HandleScopeImplementer* hsi)
    : hsi_(hsi), saved_entered_context_count_(hsi->EnteredContextCount()) {}

HandleScopeImplementer::EnteredContextRewindScope::
    ~EnteredContextRewindScope() {
  DCHECK_LE(saved_entered_context_count_, hsi_->EnteredContextCount());
  while (saved_entered_context_count_ < hsi_->EnteredContextCount()) {
    hsi_->LeaveContext();
  }
}

bool HandleScopeImplementer::LastEnteredContextWas(
    Tagged<NativeContext> context) {
  return !entered_contexts_.empty() && entered_contexts_.back() == context;
}

// If there's a spare block, use it for growing the current scope.
internal::Address* HandleScopeImplementer::GetSpareOrNewBlock() {
  internal::Address* block =
      (spare_ != nullptr) ? spare_
                          : NewArray<internal::Address>(kHandleBlockSize);
  spare_ = nullptr;
  return block;
}

void HandleScopeImplementer::DeleteExtensions(internal::Address* prev_limit) {
  while (!blocks_.empty()) {
    internal::Address* block_start = blocks_.back();
    internal::Address* block_limit = block_start + kHandleBlockSize;

    // SealHandleScope may make the prev_limit to point inside the block.
    // Cast possibly-unrelated pointers to plain Address before comparing them
    // to avoid undefined behavior.
    if (reinterpret_cast<Address>(block_start) <
            reinterpret_cast<Address>(prev_limit) &&
        reinterpret_cast<Address>(prev_limit) <=
            reinterpret_cast<Address>(block_limit)) {
#ifdef ENABLE_LOCAL_HANDLE_ZAPPING
      internal::HandleScope::ZapRange(prev_limit, block_limit);
#endif
      break;
    }

    blocks_.pop_back();
#ifdef ENABLE_LOCAL_HANDLE_ZAPPING
    internal::HandleScope::ZapRange(block_start, block_limit);
#endif
    if (spare_ != nullptr) {
      DeleteArray(spare_);
    }
    spare_ = block_start;
  }
  DCHECK((blocks_.empty() && prev_limit == nullptr) ||
         (!blocks_.empty() && prev_limit != nullptr));
}

void HandleScopeImplementer::EnterContext(Tagged<NativeContext> context) {
  entered_contexts_.push_back(context);
  Isolate::FromHandleScopeImplementer(this)->set_last_entered_context(context);
}

DirectHandle<NativeContext> HandleScopeImplementer::LastEnteredContext() {
  if (entered_contexts_.empty()) return {};
  return direct_handle(entered_contexts_.back(),
                       Isolate::FromHandleScopeImplementer(this));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_HANDLE_SCOPE_IMPLEMENTER_INL_H_
