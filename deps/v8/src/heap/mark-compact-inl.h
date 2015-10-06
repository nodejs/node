// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARK_COMPACT_INL_H_
#define V8_HEAP_MARK_COMPACT_INL_H_

#include "src/heap/mark-compact.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

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
                                      Object* target,
                                      SlotsBuffer::AdditionMode mode) {
  Page* target_page = Page::FromAddress(reinterpret_cast<Address>(target));
  if (target_page->IsEvacuationCandidate() &&
      !ShouldSkipEvacuationSlotRecording(object)) {
    if (!SlotsBuffer::AddTo(&slots_buffer_allocator_,
                            target_page->slots_buffer_address(), slot, mode)) {
      EvictPopularEvacuationCandidate(target_page);
    }
  }
}


void CodeFlusher::AddCandidate(SharedFunctionInfo* shared_info) {
  if (GetNextCandidate(shared_info) == NULL) {
    SetNextCandidate(shared_info, shared_function_info_candidates_head_);
    shared_function_info_candidates_head_ = shared_info;
  }
}


void CodeFlusher::AddCandidate(JSFunction* function) {
  DCHECK(function->code() == function->shared()->code());
  if (GetNextCandidate(function)->IsUndefined()) {
    SetNextCandidate(function, jsfunction_candidates_head_);
    jsfunction_candidates_head_ = function;
  }
}


void CodeFlusher::AddOptimizedCodeMap(SharedFunctionInfo* code_map_holder) {
  if (GetNextCodeMap(code_map_holder)->IsUndefined()) {
    SetNextCodeMap(code_map_holder, optimized_code_map_holder_head_);
    optimized_code_map_holder_head_ = code_map_holder;
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


SharedFunctionInfo* CodeFlusher::GetNextCodeMap(SharedFunctionInfo* holder) {
  FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
  Object* next_map = code_map->get(SharedFunctionInfo::kNextMapIndex);
  return reinterpret_cast<SharedFunctionInfo*>(next_map);
}


void CodeFlusher::SetNextCodeMap(SharedFunctionInfo* holder,
                                 SharedFunctionInfo* next_holder) {
  FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
  code_map->set(SharedFunctionInfo::kNextMapIndex, next_holder);
}


void CodeFlusher::ClearNextCodeMap(SharedFunctionInfo* holder) {
  FixedArray* code_map = FixedArray::cast(holder->optimized_code_map());
  code_map->set_undefined(SharedFunctionInfo::kNextMapIndex);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARK_COMPACT_INL_H_
