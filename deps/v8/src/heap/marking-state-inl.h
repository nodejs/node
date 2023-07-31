// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_STATE_INL_H_
#define V8_HEAP_MARKING_STATE_INL_H_

#include "src/heap/marking-inl.h"
#include "src/heap/marking-state.h"
#include "src/heap/memory-chunk.h"

namespace v8 {
namespace internal {

template <typename ConcreteState, AccessMode access_mode>
MarkBit MarkingStateBase<ConcreteState, access_mode>::MarkBitFrom(
    const HeapObject obj) const {
  return MarkBitFrom(BasicMemoryChunk::FromHeapObject(obj), obj.ptr());
}

template <typename ConcreteState, AccessMode access_mode>
MarkBit MarkingStateBase<ConcreteState, access_mode>::MarkBitFrom(
    const BasicMemoryChunk* p, Address addr) const {
  return MarkBit::From(addr);
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsMarked(
    const HeapObject obj) const {
  return MarkBitFrom(obj).template Get<access_mode>();
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsUnmarked(
    const HeapObject obj) const {
  return !IsMarked(obj);
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::TryMark(HeapObject obj) {
  return MarkBitFrom(obj).template Set<access_mode>();
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::TryMarkAndAccountLiveBytes(
    HeapObject obj) {
  if (TryMark(obj)) {
    static_cast<ConcreteState*>(this)->IncrementLiveBytes(
        MemoryChunk::cast(BasicMemoryChunk::FromHeapObject(obj)),
        ALIGN_TO_ALLOCATION_ALIGNMENT(obj.Size(cage_base())));
    return true;
  }
  return false;
}

template <typename ConcreteState, AccessMode access_mode>
void MarkingStateBase<ConcreteState, access_mode>::ClearLiveness(
    MemoryChunk* chunk) {
  static_cast<ConcreteState*>(this)
      ->bitmap(chunk)
      ->template Clear<access_mode>();
  static_cast<ConcreteState*>(this)->SetLiveBytes(chunk, 0);
}

MarkingBitmap* MarkingState::bitmap(MemoryChunk* chunk) const {
  return chunk->marking_bitmap();
}

void MarkingState::IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(by, kObjectAlignment8GbHeap));
  chunk->live_byte_count_.fetch_add(by, std::memory_order_relaxed);
}

intptr_t MarkingState::live_bytes(const MemoryChunk* chunk) const {
  return chunk->live_byte_count_.load(std::memory_order_relaxed);
}

void MarkingState::SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(value, kObjectAlignment8GbHeap));
  chunk->live_byte_count_.store(value, std::memory_order_relaxed);
}

MarkingBitmap* NonAtomicMarkingState::bitmap(MemoryChunk* chunk) const {
  return chunk->marking_bitmap();
}

void NonAtomicMarkingState::IncrementLiveBytes(MemoryChunk* chunk,
                                               intptr_t by) {
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(by, kObjectAlignment8GbHeap));
  chunk->live_byte_count_.fetch_add(by, std::memory_order_relaxed);
}

intptr_t NonAtomicMarkingState::live_bytes(const MemoryChunk* chunk) const {
  return chunk->live_byte_count_.load(std::memory_order_relaxed);
}

void NonAtomicMarkingState::SetLiveBytes(MemoryChunk* chunk, intptr_t value) {
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(value, kObjectAlignment8GbHeap));
  chunk->live_byte_count_.store(value, std::memory_order_relaxed);
}

MarkingBitmap* AtomicMarkingState::bitmap(MemoryChunk* chunk) const {
  return chunk->marking_bitmap();
}

void AtomicMarkingState::IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(by, kObjectAlignment8GbHeap));
  chunk->live_byte_count_.fetch_add(by);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_STATE_INL_H_
