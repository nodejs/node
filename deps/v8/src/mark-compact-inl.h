// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MARK_COMPACT_INL_H_
#define V8_MARK_COMPACT_INL_H_

#include "isolate.h"
#include "memory.h"
#include "mark-compact.h"


namespace v8 {
namespace internal {


MarkBit Marking::MarkBitFrom(Address addr) {
  MemoryChunk* p = MemoryChunk::FromAddress(addr);
  return p->markbits()->MarkBitFromIndex(p->AddressToMarkbitIndex(addr),
                                         p->ContainsOnlyData());
}


void MarkCompactCollector::SetFlags(int flags) {
  sweep_precisely_ = ((flags & Heap::kSweepPreciselyMask) != 0);
  reduce_memory_footprint_ = ((flags & Heap::kReduceMemoryFootprintMask) != 0);
  abort_incremental_marking_ =
      ((flags & Heap::kAbortIncrementalMarkingMask) != 0);
}


void MarkCompactCollector::MarkObject(HeapObject* obj, MarkBit mark_bit) {
  ASSERT(Marking::MarkBitFrom(obj) == mark_bit);
  if (!mark_bit.Get()) {
    mark_bit.Set();
    MemoryChunk::IncrementLiveBytesFromGC(obj->address(), obj->Size());
    ASSERT(IsMarked(obj));
    ASSERT(obj->GetIsolate()->heap()->Contains(obj));
    marking_deque_.PushBlack(obj);
  }
}


void MarkCompactCollector::SetMark(HeapObject* obj, MarkBit mark_bit) {
  ASSERT(!mark_bit.Get());
  ASSERT(Marking::MarkBitFrom(obj) == mark_bit);
  mark_bit.Set();
  MemoryChunk::IncrementLiveBytesFromGC(obj->address(), obj->Size());
}


bool MarkCompactCollector::IsMarked(Object* obj) {
  ASSERT(obj->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(obj);
  return Marking::MarkBitFrom(heap_object).Get();
}


void MarkCompactCollector::RecordSlot(Object** anchor_slot,
                                      Object** slot,
                                      Object* object,
                                      SlotsBuffer::AdditionMode mode) {
  Page* object_page = Page::FromAddress(reinterpret_cast<Address>(object));
  if (object_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(anchor_slot)) {
    if (!SlotsBuffer::AddTo(&slots_buffer_allocator_,
                            object_page->slots_buffer_address(),
                            slot,
                            mode)) {
      EvictEvacuationCandidate(object_page);
    }
  }
}


} }  // namespace v8::internal

#endif  // V8_MARK_COMPACT_INL_H_
