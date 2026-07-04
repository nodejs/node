// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/handle-scope-implementer.h"

#include "src/execution/isolate-data.h"
#include "src/execution/isolate.h"
#include "src/handles/handle-scope-implementer-inl.h"
#include "src/handles/persistent-handles.h"

namespace v8 {
namespace internal {

void HandleScopeImplementer::ResetAfterArchive() {
  blocks_.detach();
  entered_contexts_.detach();
  saved_contexts_.detach();
  spare_ = nullptr;
  last_handle_before_persistent_block_ = kInvalidLastHandle;
  Isolate::FromHandleScopeImplementer(this)->set_last_entered_context(
      Tagged<NativeContext>());
}

void HandleScopeImplementer::Free() {
  DCHECK(blocks_.empty());
  DCHECK(entered_contexts_.empty());
  DCHECK(saved_contexts_.empty());

  blocks_.free();
  entered_contexts_.free();
  saved_contexts_.free();
  if (spare_ != nullptr) {
    DeleteArray(spare_);
    spare_ = nullptr;
  }
  DCHECK(Isolate::FromHandleScopeImplementer(this)
             ->thread_local_top()
             ->CallDepthIsZero());
}

void HandleScopeImplementer::BeginPersistentScope() {
  DCHECK_EQ(last_handle_before_persistent_block_, kInvalidLastHandle);
  last_handle_before_persistent_block_ =
      Isolate::FromHandleScopeImplementer(this)->handle_scope_data()->next;
}

void HandleScopeImplementer::FreeThreadResources() { Free(); }

char* HandleScopeImplementer::ArchiveThread(char* storage) {
  HandleScopeData* current =
      Isolate::FromHandleScopeImplementer(this)->handle_scope_data();
  handle_scope_data_ = *current;
  MemCopy(storage, this, sizeof(*this));

  ResetAfterArchive();
  current->Initialize();

  return storage + ArchiveSpacePerThread();
}

int HandleScopeImplementer::ArchiveSpacePerThread() {
  return sizeof(HandleScopeImplementer);
}

char* HandleScopeImplementer::RestoreThread(char* storage) {
  MemCopy(this, storage, sizeof(*this));
  *Isolate::FromHandleScopeImplementer(this)->handle_scope_data() =
      handle_scope_data_;
  Isolate::FromHandleScopeImplementer(this)->set_last_entered_context(
      entered_contexts_.empty() ? Tagged<NativeContext>()
                                : entered_contexts_.back());
  return storage + ArchiveSpacePerThread();
}

void HandleScopeImplementer::IterateThis(RootVisitor* v) {
#ifdef DEBUG
  bool found_block_before_persistent = false;
#endif
  // Iterate over all handles in the blocks except for the last.
  for (int i = static_cast<int>(blocks()->size()) - 2; i >= 0; --i) {
    Address* block = blocks()->at(i);
    // Cast possibly-unrelated pointers to plain Address before comparing them
    // to avoid undefined behavior.
    if (HasPersistentScope() &&
        (reinterpret_cast<Address>(last_handle_before_persistent_block_) <=
         reinterpret_cast<Address>(&block[kHandleBlockSize])) &&
        (reinterpret_cast<Address>(last_handle_before_persistent_block_) >=
         reinterpret_cast<Address>(block))) {
      v->VisitRootPointers(
          Root::kHandleScope, nullptr, FullObjectSlot(block),
          FullObjectSlot(last_handle_before_persistent_block_));
      DCHECK(!found_block_before_persistent);
#ifdef DEBUG
      found_block_before_persistent = true;
#endif
    } else {
      v->VisitRootPointers(Root::kHandleScope, nullptr, FullObjectSlot(block),
                           FullObjectSlot(&block[kHandleBlockSize]));
    }
  }

  DCHECK_EQ(
      HasPersistentScope() && last_handle_before_persistent_block_ != nullptr,
      found_block_before_persistent);

  // Iterate over live handles in the last block (if any).
  if (!blocks()->empty()) {
    v->VisitRootPointers(Root::kHandleScope, nullptr,
                         FullObjectSlot(blocks()->back()),
                         FullObjectSlot(handle_scope_data_.next));
  }

  saved_contexts_.shrink_to_fit();
  if (!saved_contexts_.empty()) {
    FullObjectSlot start(&saved_contexts_.front());
    v->VisitRootPointers(Root::kHandleScope, nullptr, start,
                         start + static_cast<int>(saved_contexts_.size()));
  }
  entered_contexts_.shrink_to_fit();
  if (!entered_contexts_.empty()) {
    FullObjectSlot start(&entered_contexts_.front());
    v->VisitRootPointers(Root::kHandleScope, nullptr, start,
                         start + static_cast<int>(entered_contexts_.size()));
  }
}

void HandleScopeImplementer::Iterate(RootVisitor* v) {
  HandleScopeData* current =
      Isolate::FromHandleScopeImplementer(this)->handle_scope_data();
  handle_scope_data_ = *current;
  IterateThis(v);
}

char* HandleScopeImplementer::Iterate(RootVisitor* v, char* storage) {
  HandleScopeImplementer* scope_implementer =
      reinterpret_cast<HandleScopeImplementer*>(storage);
  scope_implementer->IterateThis(v);
  return storage + ArchiveSpacePerThread();
}

std::unique_ptr<PersistentHandles> HandleScopeImplementer::DetachPersistent(
    Address* first_block) {
  std::unique_ptr<PersistentHandles> ph(
      new PersistentHandles(Isolate::FromHandleScopeImplementer(this)));
  DCHECK(HasPersistentScope());
  DCHECK_NOT_NULL(first_block);

  Address* block_start;
  do {
    block_start = blocks_.back();
    ph->blocks_.push_back(blocks_.back());
#if DEBUG
    ph->ordered_blocks_.insert(blocks_.back());
#endif
    blocks_.pop_back();
  } while (block_start != first_block);

  // ph->blocks_ now contains the blocks installed on the HandleScope stack
  // since BeginPersistentScope was called, but in reverse order.

  // Switch first and last blocks, such that the last block is the one
  // that is potentially half full.
  DCHECK(!ph->blocks_.empty());
  std::swap(ph->blocks_.front(), ph->blocks_.back());

  ph->block_next_ =
      Isolate::FromHandleScopeImplementer(this)->handle_scope_data()->next;
  block_start = ph->blocks_.back();
  ph->block_limit_ = block_start + kHandleBlockSize;

  DCHECK_EQ(blocks_.empty(), last_handle_before_persistent_block_ == nullptr);
  last_handle_before_persistent_block_ = kInvalidLastHandle;
  return ph;
}

}  // namespace internal
}  // namespace v8
