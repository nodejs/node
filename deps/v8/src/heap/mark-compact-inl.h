// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/heap/mark-compact.h"
#include "src/heap/remembered-set.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

void MarkCompactCollector::PushBlack(HeapObject* obj) {
  DCHECK(Marking::IsBlack(ObjectMarking::MarkBitFrom(obj)));
  if (marking_deque()->Push(obj)) {
    MemoryChunk::IncrementLiveBytes(obj, obj->Size());
  } else {
    MarkBit mark_bit = ObjectMarking::MarkBitFrom(obj);
    Marking::BlackToGrey(mark_bit);
  }
}


void MarkCompactCollector::UnshiftBlack(HeapObject* obj) {
  DCHECK(Marking::IsBlack(ObjectMarking::MarkBitFrom(obj)));
  if (!marking_deque()->Unshift(obj)) {
    MemoryChunk::IncrementLiveBytes(obj, -obj->Size());
    MarkBit mark_bit = ObjectMarking::MarkBitFrom(obj);
    Marking::BlackToGrey(mark_bit);
  }
}


void MarkCompactCollector::MarkObject(HeapObject* obj, MarkBit mark_bit) {
  DCHECK(ObjectMarking::MarkBitFrom(obj) == mark_bit);
  if (Marking::IsWhite(mark_bit)) {
    Marking::WhiteToBlack(mark_bit);
    DCHECK(obj->GetIsolate()->heap()->Contains(obj));
    PushBlack(obj);
  }
}


void MarkCompactCollector::SetMark(HeapObject* obj, MarkBit mark_bit) {
  DCHECK(Marking::IsWhite(mark_bit));
  DCHECK(ObjectMarking::MarkBitFrom(obj) == mark_bit);
  Marking::WhiteToBlack(mark_bit);
  MemoryChunk::IncrementLiveBytes(obj, obj->Size());
}


bool MarkCompactCollector::IsMarked(Object* obj) {
  DCHECK(obj->IsHeapObject());
  HeapObject* heap_object = HeapObject::cast(obj);
  return Marking::IsBlackOrGrey(ObjectMarking::MarkBitFrom(heap_object));
}


void MarkCompactCollector::RecordSlot(HeapObject* object, Object** slot,
                                      Object* target) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(object));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(object)) {
    DCHECK(Marking::IsBlackOrGrey(ObjectMarking::MarkBitFrom(object)));
    RememberedSet<OLD_TO_OLD>::Insert(source_page,
                                      reinterpret_cast<Address>(slot));
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
  if (function->next_function_link()->IsUndefined(isolate_)) {
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
  DCHECK(undefined->IsUndefined(candidate->GetIsolate()));
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
        // However, if there is a black area at the end of the page, and the
        // last word is a one word filler, we are not allowed to advance. In
        // that case we can return immediately.
        if (it_.Done()) {
          DCHECK(HeapObject::FromAddress(addr)->map() ==
                 HeapObject::FromAddress(addr)
                     ->GetHeap()
                     ->one_pointer_filler_map());
          return nullptr;
        }
        it_.Advance();
        cell_base_ = it_.CurrentCellBase();
        current_cell_ = *it_.CurrentCell();
      }

      Map* map = nullptr;
      if (current_cell_ & second_bit_index) {
        // We found a black object. If the black object is within a black area,
        // make sure that we skip all set bits in the black area until the
        // object ends.
        HeapObject* black_object = HeapObject::FromAddress(addr);
        map = base::NoBarrierAtomicValue<Map*>::FromAddress(addr)->Value();
        Address end = addr + black_object->SizeFromMap(map) - kPointerSize;
        // One word filler objects do not borrow the second mark bit. We have
        // to jump over the advancing and clearing part.
        // Note that we know that we are at a one word filler when
        // object_start + object_size - kPointerSize == object_start.
        if (addr != end) {
          DCHECK_EQ(chunk_, MemoryChunk::FromAddress(end));
          uint32_t end_mark_bit_index = chunk_->AddressToMarkbitIndex(end);
          unsigned int end_cell_index =
              end_mark_bit_index >> Bitmap::kBitsPerCellLog2;
          MarkBit::CellType end_index_mask =
              1u << Bitmap::IndexInCell(end_mark_bit_index);
          if (it_.Advance(end_cell_index)) {
            cell_base_ = it_.CurrentCellBase();
            current_cell_ = *it_.CurrentCell();
          }

          // Clear all bits in current_cell, including the end index.
          current_cell_ &= ~(end_index_mask + end_index_mask - 1);
        }

        if (T == kBlackObjects || T == kAllLiveObjects) {
          object = black_object;
        }
      } else if ((T == kGreyObjects || T == kAllLiveObjects)) {
        map = base::NoBarrierAtomicValue<Map*>::FromAddress(addr)->Value();
        object = HeapObject::FromAddress(addr);
      }

      // We found a live object.
      if (object != nullptr) {
        if (map == heap()->one_pointer_filler_map()) {
          // Black areas together with slack tracking may result in black one
          // word filler objects. We filter these objects out in the iterator.
          object = nullptr;
        } else {
          break;
        }
      }
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
