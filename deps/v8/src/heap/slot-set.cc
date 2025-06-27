// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/slot-set.h"

#include "src/base/logging.h"
#include "src/heap/memory-chunk-layout.h"

namespace v8 {
namespace internal {

TypedSlots::~TypedSlots() {
  Chunk* chunk = head_;
  while (chunk != nullptr) {
    Chunk* next = chunk->next;
    delete chunk;
    chunk = next;
  }
  head_ = nullptr;
  tail_ = nullptr;
}

void TypedSlots::Insert(SlotType type, uint32_t offset) {
  TypedSlot slot = {TypeField::encode(type) | OffsetField::encode(offset)};
  Chunk* chunk = EnsureChunk();
  DCHECK_LT(chunk->buffer.size(), chunk->buffer.capacity());
  chunk->buffer.push_back(slot);
}

void TypedSlots::Merge(TypedSlots* other) {
  if (other->head_ == nullptr) {
    return;
  }
  if (head_ == nullptr) {
    head_ = other->head_;
    tail_ = other->tail_;
  } else {
    tail_->next = other->head_;
    tail_ = other->tail_;
  }
  other->head_ = nullptr;
  other->tail_ = nullptr;
}

TypedSlots::Chunk* TypedSlots::EnsureChunk() {
  if (!head_) {
    head_ = tail_ = NewChunk(nullptr, kInitialBufferSize);
  }
  if (head_->buffer.size() == head_->buffer.capacity()) {
    head_ = NewChunk(head_, NextCapacity(head_->buffer.capacity()));
  }
  return head_;
}

TypedSlots::Chunk* TypedSlots::NewChunk(Chunk* next, size_t capacity) {
  Chunk* chunk = new Chunk;
  chunk->next = next;
  chunk->buffer.reserve(capacity);
  DCHECK_EQ(chunk->buffer.capacity(), capacity);
  return chunk;
}

void TypedSlotSet::ClearInvalidSlots(const FreeRangesMap& invalid_ranges) {
  IterateSlotsInRanges([](TypedSlot* slot) { *slot = ClearedTypedSlot(); },
                       invalid_ranges);
}

void TypedSlotSet::AssertNoInvalidSlots(const FreeRangesMap& invalid_ranges) {
  IterateSlotsInRanges(
      [](TypedSlot* slot) {
        CHECK_WITH_MSG(false, "No slot in ranges expected.");
      },
      invalid_ranges);
}

template <typename Callback>
void TypedSlotSet::IterateSlotsInRanges(Callback callback,
                                        const FreeRangesMap& ranges) {
  if (ranges.empty()) return;

  Chunk* chunk = LoadHead();
  while (chunk != nullptr) {
    for (TypedSlot& slot : chunk->buffer) {
      SlotType type = TypeField::decode(slot.type_and_offset);
      if (type == SlotType::kCleared) continue;
      uint32_t offset = OffsetField::decode(slot.type_and_offset);
      FreeRangesMap::const_iterator upper_bound = ranges.upper_bound(offset);
      if (upper_bound == ranges.begin()) continue;
      // upper_bounds points to the invalid range after the given slot. Hence,
      // we have to go to the previous element.
      upper_bound--;
      DCHECK_LE(upper_bound->first, offset);
      if (upper_bound->second > offset) {
        callback(&slot);
      }
    }
    chunk = LoadNext(chunk);
  }
}

}  // namespace internal
}  // namespace v8
