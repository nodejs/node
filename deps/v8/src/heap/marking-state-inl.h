// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_STATE_INL_H_
#define V8_HEAP_MARKING_STATE_INL_H_

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
  return static_cast<const ConcreteState*>(this)->bitmap(p)->MarkBitFromIndex(
      p->AddressToMarkbitIndex(addr));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsImpossible(
    const HeapObject obj) const {
  return Marking::IsImpossible<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsMarked(
    const HeapObject obj) const {
  return Marking::IsBlack<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsUnmarked(
    const HeapObject obj) const {
  return Marking::IsWhite<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsGrey(
    const HeapObject obj) const {
  return Marking::IsGrey<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::IsBlackOrGrey(
    const HeapObject obj) const {
  return Marking::IsBlackOrGrey<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::TryMark(HeapObject obj) {
  return Marking::WhiteToGrey<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::TryMarkAndAccountLiveBytes(
    HeapObject obj) {
  if (TryMark(obj) && GreyToBlack(obj)) {
    static_cast<ConcreteState*>(this)->IncrementLiveBytes(
        MemoryChunk::cast(BasicMemoryChunk::FromHeapObject(obj)),
        ALIGN_TO_ALLOCATION_ALIGNMENT(obj.Size(cage_base())));
    return true;
  }
  return false;
}

template <typename ConcreteState, AccessMode access_mode>
bool MarkingStateBase<ConcreteState, access_mode>::GreyToBlack(HeapObject obj) {
  return Marking::GreyToBlack<access_mode>(MarkBitFrom(obj));
}

template <typename ConcreteState, AccessMode access_mode>
void MarkingStateBase<ConcreteState, access_mode>::ClearLiveness(
    MemoryChunk* chunk) {
  static_cast<ConcreteState*>(this)->bitmap(chunk)->Clear();
  static_cast<ConcreteState*>(this)->SetLiveBytes(chunk, 0);
}

ConcurrentBitmap<AccessMode::ATOMIC>* MarkingState::bitmap(
    const BasicMemoryChunk* chunk) const {
  return chunk->marking_bitmap<AccessMode::ATOMIC>();
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

ConcurrentBitmap<AccessMode::NON_ATOMIC>* NonAtomicMarkingState::bitmap(
    const BasicMemoryChunk* chunk) const {
  return chunk->marking_bitmap<AccessMode::NON_ATOMIC>();
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

ConcurrentBitmap<AccessMode::ATOMIC>* AtomicMarkingState::bitmap(
    const BasicMemoryChunk* chunk) const {
  return chunk->marking_bitmap<AccessMode::ATOMIC>();
}

void AtomicMarkingState::IncrementLiveBytes(MemoryChunk* chunk, intptr_t by) {
  DCHECK_IMPLIES(V8_COMPRESS_POINTERS_8GB_BOOL,
                 IsAligned(by, kObjectAlignment8GbHeap));
  chunk->live_byte_count_.fetch_add(by);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_STATE_INL_H_
