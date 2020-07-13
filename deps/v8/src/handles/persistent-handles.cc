// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/persistent-handles.h"

#include "src/api/api.h"
#include "src/heap/heap-inl.h"
#include "src/heap/safepoint.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

PersistentHandles::PersistentHandles(Isolate* isolate, size_t block_size)
    : isolate_(isolate),
      block_size_(block_size),
      block_next_(nullptr),
      block_limit_(nullptr),
      prev_(nullptr),
      next_(nullptr) {
  isolate->persistent_handles_list()->Add(this);
}

PersistentHandles::~PersistentHandles() {
  isolate_->persistent_handles_list()->Remove(this);

  for (Address* block_start : blocks_) {
    DeleteArray(block_start);
  }
}

#ifdef DEBUG
void PersistentHandles::Attach(LocalHeap* local_heap) {
  DCHECK_NULL(owner_);
  owner_ = local_heap;
}

void PersistentHandles::Detach() {
  DCHECK_NOT_NULL(owner_);
  owner_ = nullptr;
}
#endif

void PersistentHandles::AddBlock() {
  DCHECK_EQ(block_next_, block_limit_);

  Address* block_start = NewArray<Address>(block_size_);
  blocks_.push_back(block_start);

  block_next_ = block_start;
  block_limit_ = block_start + block_size_;
}

Handle<Object> PersistentHandles::NewHandle(Address value) {
#ifdef DEBUG
  if (owner_) DCHECK(!owner_->IsParked());
#endif
  return Handle<Object>(GetHandle(value));
}

Address* PersistentHandles::GetHandle(Address value) {
  if (block_next_ == block_limit_) {
    AddBlock();
  }

  DCHECK_LT(block_next_, block_limit_);
  *block_next_ = value;
  return block_next_++;
}

void PersistentHandles::Iterate(RootVisitor* visitor) {
  for (int i = 0; i < static_cast<int>(blocks_.size()) - 1; i++) {
    Address* block_start = blocks_[i];
    Address* block_end = block_start + block_size_;
    visitor->VisitRootPointers(Root::kHandleScope, nullptr,
                               FullObjectSlot(block_start),
                               FullObjectSlot(block_end));
  }

  if (!blocks_.empty()) {
    Address* block_start = blocks_.back();
    visitor->VisitRootPointers(Root::kHandleScope, nullptr,
                               FullObjectSlot(block_start),
                               FullObjectSlot(block_next_));
  }
}

void PersistentHandlesList::Add(PersistentHandles* persistent_handles) {
  base::MutexGuard guard(&persistent_handles_mutex_);
  if (persistent_handles_head_)
    persistent_handles_head_->prev_ = persistent_handles;
  persistent_handles->prev_ = nullptr;
  persistent_handles->next_ = persistent_handles_head_;
  persistent_handles_head_ = persistent_handles;
}

void PersistentHandlesList::Remove(PersistentHandles* persistent_handles) {
  base::MutexGuard guard(&persistent_handles_mutex_);
  if (persistent_handles->next_)
    persistent_handles->next_->prev_ = persistent_handles->prev_;
  if (persistent_handles->prev_)
    persistent_handles->prev_->next_ = persistent_handles->next_;
  else
    persistent_handles_head_ = persistent_handles->next_;
}

void PersistentHandlesList::Iterate(RootVisitor* visitor) {
#if DEBUG
  DCHECK(isolate_->heap()->safepoint()->IsActive());
#else
  USE(isolate_);
#endif
  base::MutexGuard guard(&persistent_handles_mutex_);
  for (PersistentHandles* current = persistent_handles_head_; current;
       current = current->next_) {
    current->Iterate(visitor);
  }
}

}  // namespace internal
}  // namespace v8
