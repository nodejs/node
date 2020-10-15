// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_HEAP_MARKING_WORKLIST_INL_H_
#define V8_HEAP_MARKING_WORKLIST_INL_H_

#include <unordered_map>
#include <vector>

#include "src/heap/marking-worklist.h"

namespace v8 {
namespace internal {

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Push(Segment* segment) {
  base::MutexGuard guard(&lock_);
  segment->set_next(top_);
  set_top(segment);
  size_.fetch_add(1, std::memory_order_relaxed);
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::Pop(Segment** segment) {
  base::MutexGuard guard(&lock_);
  if (top_ != nullptr) {
    DCHECK_LT(0U, size_);
    size_.fetch_sub(1, std::memory_order_relaxed);
    *segment = top_;
    set_top(top_->next());
    return true;
  }
  return false;
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::IsEmpty() {
  return base::AsAtomicPointer::Relaxed_Load(&top_) == nullptr;
}

template <typename EntryType, int SegmentSize>
size_t MarkingWorklistImpl<EntryType, SegmentSize>::Size() {
  // It is safe to read |size_| without a lock since this variable is
  // atomic, keeping in mind that threads may not immediately see the new
  // value when it is updated.
  return size_.load(std::memory_order_relaxed);
}

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Clear() {
  base::MutexGuard guard(&lock_);
  size_.store(0, std::memory_order_relaxed);
  Segment* current = top_;
  while (current != nullptr) {
    Segment* tmp = current;
    current = current->next();
    delete tmp;
  }
  set_top(nullptr);
}

template <typename EntryType, int SegmentSize>
template <typename Callback>
void MarkingWorklistImpl<EntryType, SegmentSize>::Update(Callback callback) {
  base::MutexGuard guard(&lock_);
  Segment* prev = nullptr;
  Segment* current = top_;
  size_t num_deleted = 0;
  while (current != nullptr) {
    current->Update(callback);
    if (current->IsEmpty()) {
      DCHECK_LT(0U, size_);
      ++num_deleted;
      if (prev == nullptr) {
        top_ = current->next();
      } else {
        prev->set_next(current->next());
      }
      Segment* tmp = current;
      current = current->next();
      delete tmp;
    } else {
      prev = current;
      current = current->next();
    }
  }
  size_.fetch_sub(num_deleted, std::memory_order_relaxed);
}

template <typename EntryType, int SegmentSize>
template <typename Callback>
void MarkingWorklistImpl<EntryType, SegmentSize>::Iterate(Callback callback) {
  base::MutexGuard guard(&lock_);
  for (Segment* current = top_; current != nullptr; current = current->next()) {
    current->Iterate(callback);
  }
}

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Merge(
    MarkingWorklistImpl<EntryType, SegmentSize>* other) {
  Segment* top = nullptr;
  size_t other_size = 0;
  {
    base::MutexGuard guard(&other->lock_);
    if (!other->top_) return;
    top = other->top_;
    other_size = other->size_.load(std::memory_order_relaxed);
    other->size_.store(0, std::memory_order_relaxed);
    other->set_top(nullptr);
  }

  // It's safe to iterate through these segments because the top was
  // extracted from |other|.
  Segment* end = top;
  while (end->next()) end = end->next();

  {
    base::MutexGuard guard(&lock_);
    size_.fetch_add(other_size, std::memory_order_relaxed);
    end->set_next(top_);
    set_top(top);
  }
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::Segment::Push(
    EntryType entry) {
  if (IsFull()) return false;
  entries_[index_++] = entry;
  return true;
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::Segment::Pop(
    EntryType* entry) {
  if (IsEmpty()) return false;
  *entry = entries_[--index_];
  return true;
}

template <typename EntryType, int SegmentSize>
template <typename Callback>
void MarkingWorklistImpl<EntryType, SegmentSize>::Segment::Update(
    Callback callback) {
  size_t new_index = 0;
  for (size_t i = 0; i < index_; i++) {
    if (callback(entries_[i], &entries_[new_index])) {
      new_index++;
    }
  }
  index_ = new_index;
}

template <typename EntryType, int SegmentSize>
template <typename Callback>
void MarkingWorklistImpl<EntryType, SegmentSize>::Segment::Iterate(
    Callback callback) const {
  for (size_t i = 0; i < index_; i++) {
    callback(entries_[i]);
  }
}

template <typename EntryType, int SegmentSize>
MarkingWorklistImpl<EntryType, SegmentSize>::Local::Local(
    MarkingWorklistImpl<EntryType, SegmentSize>* worklist)
    : worklist_(worklist),
      push_segment_(NewSegment()),
      pop_segment_(NewSegment()) {}

template <typename EntryType, int SegmentSize>
MarkingWorklistImpl<EntryType, SegmentSize>::Local::~Local() {
  CHECK_IMPLIES(push_segment_, push_segment_->IsEmpty());
  CHECK_IMPLIES(pop_segment_, pop_segment_->IsEmpty());
  delete push_segment_;
  delete pop_segment_;
}

template <typename EntryType, int SegmentSize>
MarkingWorklistImpl<EntryType, SegmentSize>::Local::Local(
    MarkingWorklistImpl<EntryType, SegmentSize>::Local&& other) V8_NOEXCEPT {
  worklist_ = other.worklist_;
  push_segment_ = other.push_segment_;
  pop_segment_ = other.pop_segment_;
  other.worklist_ = nullptr;
  other.push_segment_ = nullptr;
  other.pop_segment_ = nullptr;
}

template <typename EntryType, int SegmentSize>
typename MarkingWorklistImpl<EntryType, SegmentSize>::Local&
MarkingWorklistImpl<EntryType, SegmentSize>::Local::operator=(
    MarkingWorklistImpl<EntryType, SegmentSize>::Local&& other) V8_NOEXCEPT {
  if (this != &other) {
    DCHECK_NULL(worklist_);
    DCHECK_NULL(push_segment_);
    DCHECK_NULL(pop_segment_);
    worklist_ = other.worklist_;
    push_segment_ = other.push_segment_;
    pop_segment_ = other.pop_segment_;
    other.worklist_ = nullptr;
    other.push_segment_ = nullptr;
    other.pop_segment_ = nullptr;
  }
  return *this;
}

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Local::Push(EntryType entry) {
  if (V8_UNLIKELY(!push_segment_->Push(entry))) {
    PublishPushSegment();
    bool success = push_segment_->Push(entry);
    USE(success);
    DCHECK(success);
  }
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::Local::Pop(EntryType* entry) {
  if (!pop_segment_->Pop(entry)) {
    if (!push_segment_->IsEmpty()) {
      std::swap(push_segment_, pop_segment_);
    } else if (!StealPopSegment()) {
      return false;
    }
    bool success = pop_segment_->Pop(entry);
    USE(success);
    DCHECK(success);
  }
  return true;
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::Local::IsLocalEmpty() const {
  return push_segment_->IsEmpty() && pop_segment_->IsEmpty();
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::Local::IsGlobalEmpty() const {
  return worklist_->IsEmpty();
}

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Local::Publish() {
  if (!push_segment_->IsEmpty()) {
    PublishPushSegment();
  }
  if (!pop_segment_->IsEmpty()) {
    PublishPopSegment();
  }
}

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Local::Merge(
    MarkingWorklistImpl<EntryType, SegmentSize>::Local* other) {
  other->Publish();
  worklist_->Merge(other->worklist_);
}

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Local::PublishPushSegment() {
  worklist_->Push(push_segment_);
  push_segment_ = NewSegment();
}

template <typename EntryType, int SegmentSize>
void MarkingWorklistImpl<EntryType, SegmentSize>::Local::PublishPopSegment() {
  worklist_->Push(pop_segment_);
  pop_segment_ = NewSegment();
}

template <typename EntryType, int SegmentSize>
bool MarkingWorklistImpl<EntryType, SegmentSize>::Local::StealPopSegment() {
  if (worklist_->IsEmpty()) return false;
  Segment* new_segment = nullptr;
  if (worklist_->Pop(&new_segment)) {
    delete pop_segment_;
    pop_segment_ = new_segment;
    return true;
  }
  return false;
}

template <typename Callback>
void MarkingWorklists::Update(Callback callback) {
  shared_.Update(callback);
  on_hold_.Update(callback);
  embedder_.Update(callback);
  other_.Update(callback);
  for (auto cw : context_worklists_) {
    if (cw.context == kSharedContext || cw.context == kOtherContext) {
      // These contexts were updated above.
      continue;
    }
    cw.worklist->Update(callback);
  }
}

void MarkingWorklists::Local::Push(HeapObject object) { active_.Push(object); }

bool MarkingWorklists::Local::Pop(HeapObject* object) {
  if (active_.Pop(object)) return true;
  if (!is_per_context_mode_) return false;
  // The active worklist is empty. Find any other non-empty worklist and
  // switch the active worklist to it.
  return PopContext(object);
}

void MarkingWorklists::Local::PushOnHold(HeapObject object) {
  on_hold_.Push(object);
}

bool MarkingWorklists::Local::PopOnHold(HeapObject* object) {
  return on_hold_.Pop(object);
}

void MarkingWorklists::Local::PushEmbedder(HeapObject object) {
  embedder_.Push(object);
}

bool MarkingWorklists::Local::PopEmbedder(HeapObject* object) {
  return embedder_.Pop(object);
}

Address MarkingWorklists::Local::SwitchToContext(Address context) {
  if (context == active_context_) return context;
  return SwitchToContextSlow(context);
}

Address MarkingWorklists::Local::SwitchToShared() {
  return SwitchToContext(kSharedContext);
}

void MarkingWorklists::Local::SwitchToContext(
    Address context, MarkingWorklist::Local* worklist) {
  // Save the current worklist.
  *active_owner_ = std::move(active_);
  // Switch to the new worklist.
  active_owner_ = worklist;
  active_ = std::move(*worklist);
  active_context_ = context;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_WORKLIST_INL_H_
