// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/heap/mark-compact.h"
#include "src/heap/slots-buffer.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

inline std::vector<Page*>& MarkCompactCollector::sweeping_list(Space* space) {
  if (space == heap()->old_space()) {
    return sweeping_list_old_space_;
  } else if (space == heap()->code_space()) {
    return sweeping_list_code_space_;
  }
  DCHECK_EQ(space, heap()->map_space());
  return sweeping_list_map_space_;
}


void MarkCompactCollector::PushBlack(HeapObject* obj) {
  DCHECK(Marking::IsBlack(Marking::MarkBitFrom(obj)));
  if (marking_deque_.Push(obj)) {
    MemoryChunk::IncrementLiveBytesFromGC(obj, obj->Size());
  } else {
    Marking::BlackToGrey(obj);
  }
}


void MarkCompactCollector::UnshiftBlack(HeapObject* obj) {
  DCHECK(Marking::IsBlack(Marking::MarkBitFrom(obj)));
  if (!marking_deque_.Unshift(obj)) {
    MemoryChunk::IncrementLiveBytesFromGC(obj, -obj->Size());
    Marking::BlackToGrey(obj);
  }
}


void MarkCompactCollector::MarkObject(HeapObject* obj, MarkBit mark_bit) {
  DCHECK(Marking::MarkBitFrom(obj) == mark_bit);
  if (Marking::IsWhite(mark_bit)) {
    Marking::WhiteToBlack(mark_bit);
    DCHECK(obj->GetIsolate()->heap()->Contains(obj));
    PushBlack(obj);
  }
}


void MarkCompactCollector::SetMark(HeapObject* obj, MarkBit mark_bit) {
  DCHECK(Marking::IsWhite(mark_bit));
  DCHECK(Marking::MarkBitFrom(obj) == mark_bit);
  Marking::WhiteToBlack(mark_bit);
  MemoryChunk::IncrementLiveBytesFromGC(obj, obj->Size());
}


bool MarkCompactCollector::IsMarked(Object* obj) {
  DCHECK(obj->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(obj);
  return Marking::IsBlackOrGrey(Marking::MarkBitFrom(heap_object));
}


void MarkCompactCollector::RecordSlot(HeapObject* object, Object** slot,
                                      Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(object)) {
    if (!SlotsBuffer::AddTo(slots_buffer_allocator_,
                            target_page->slots_buffer_address(), slot,
                            SlotsBuffer::FAIL_ON_OVERFLOW)) {
      EvictPopularEvacuationCandidate(target_page);
    }
  }
}


void MarkCompactCollector::ForceRecordSlot(HeapObject* object, Object** slot,
                                           Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(object)) {
    CHECK(SlotsBuffer::AddTo(slots_buffer_allocator_,
                             target_page->slots_buffer_address(), slot,
                             SlotsBuffer::IGNORE_OVERFLOW));
  }
}


void CodeFlusher::AddCandidate(SharedFunctionInfo* shared_info) {
  if (GetNextCandidate(shared_info) == nullptr) {
    SetNextCandidate(shared_info, shared_function_info_candidates_head_);
    shared_function_info_candidates_head_ = shared_info;
  }
}


void CodeFlusher::AddCandidate(JSFunction* function) {
  DCHECK(function->code() == function->shared()->code());
  if (function->next_function_link()->IsUndefined()) {
    SetNextCandidate(function, jsfunction_candidates_head_);
    jsfunction_candidates_head_ = function;
  }
}


JSFunction** CodeFlusher::GetNextCandidateSlot(JSFunction* candidate) {
  return reinterpret_cast<JSFunction**>(
      HeapObject::RawField(candidate, JSFunction::kNextFunctionLinkOffset));
}


JSFunction* CodeFlusher::GetNextCandidate(JSFunction* candidate) {
  Object* next_candidate = candidate->next_function_link();
  return reinterpret_cast<JSFunction*>(next_candidate);
}


void CodeFlusher::SetNextCandidate(JSFunction* candidate,
                                   JSFunction* next_candidate) {
  candidate->set_next_function_link(next_candidate, UPDATE_WEAK_WRITE_BARRIER);
}


void CodeFlusher::ClearNextCandidate(JSFunction* candidate, Object* undefined) {
  DCHECK(undefined->IsUndefined());
  candidate->set_next_function_link(undefined, SKIP_WRITE_BARRIER);
}


SharedFunctionInfo* CodeFlusher::GetNextCandidate(
    SharedFunctionInfo* candidate) {
  Object* next_candidate = candidate->code()->gc_metadata();
  return reinterpret_cast<SharedFunctionInfo*>(next_candidate);
}


void CodeFlusher::SetNextCandidate(SharedFunctionInfo* candidate,
                                   SharedFunctionInfo* next_candidate) {
  candidate->code()->set_gc_metadata(next_candidate);
}


void CodeFlusher::ClearNextCandidate(SharedFunctionInfo* candidate) {
  candidate->code()->set_gc_metadata(NULL, SKIP_WRITE_BARRIER);
}


template <LiveObjectIterationMode T>
HeapObject* LiveObjectIterator<T>::Next() {
  while (!it_.Done()) {
    HeapObject* object = nullptr;
    while (current_cell_ != 0) {
      uint32_t trailing_zeros = base::bits::CountTrailingZeros32(current_cell_);
      Address addr = cell_base_ + trailing_zeros * kPointerSize;

      // Clear the first bit of the found object..
      current_cell_ &= ~(1u << trailing_zeros);

      uint32_t second_bit_index = 0;
      if (trailing_zeros < Bitmap::kBitIndexMask) {
        second_bit_index = 1u << (trailing_zeros + 1);
      } else {
        second_bit_index = 0x1;
        // The overlapping case; there has to exist a cell after the current
        // cell.
        DCHECK(!it_.Done());
        it_.Advance();
        cell_base_ = it_.CurrentCellBase();
        current_cell_ = *it_.CurrentCell();
      }
      if (T == kBlackObjects && (current_cell_ & second_bit_index)) {
        object = HeapObject::FromAddress(addr);
      } else if (T == kGreyObjects && !(current_cell_ & second_bit_index)) {
        object = HeapObject::FromAddress(addr);
      } else if (T == kAllLiveObjects) {
        object = HeapObject::FromAddress(addr);
      }
      // Clear the second bit of the found object.
      current_cell_ &= ~second_bit_index;

      // We found a live object.
      if (object != nullptr) break;
    }
    if (current_cell_ == 0) {
      if (!it_.Done()) {
        it_.Advance();
        cell_base_ = it_.CurrentCellBase();
        current_cell_ = *it_.CurrentCell();
      }
    }
    if (object != nullptr) return object;
  }
  return nullptr;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
