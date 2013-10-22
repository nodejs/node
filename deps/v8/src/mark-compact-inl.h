// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
                                      Object* object) {
  Page* object_page = Page::FromAddress(reinterpret_cast<Address>(object));
  if (object_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(anchor_slot)) {
    if (!SlotsBuffer::AddTo(&slots_buffer_allocator_,
                            object_page->slots_buffer_address(),
                            slot,
                            SlotsBuffer::FAIL_ON_OVERFLOW)) {
      EvictEvacuationCandidate(object_page);
    }
  }
}


} }  // namespace v8::internal

#endif  // V8_MARK_COMPACT_INL_H_
