// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_INL_H_
#define V8_HEAP_INCREMENTAL_MARKING_INL_H_

#include "src/heap/incremental-marking.h"

namespace v8 {
namespace internal {


bool IncrementalMarking::BaseRecordWrite(HeapObject* obj, Object** slot,
                                         Object* value) {
  HeapObject* value_heap_obj = HeapObject::cast(value);
  MarkBit value_bit = Marking::MarkBitFrom(value_heap_obj);
  if (Marking::IsWhite(value_bit)) {
    MarkBit obj_bit = Marking::MarkBitFrom(obj);
    if (Marking::IsBlack(obj_bit)) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
      if (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
        if (chunk->IsLeftOfProgressBar(slot)) {
          WhiteToGreyAndPush(value_heap_obj, value_bit);
          RestartIfNotMarking();
        } else {
          return false;
        }
      } else {
        BlackToGreyAndUnshift(obj, obj_bit);
        RestartIfNotMarking();
        return false;
      }
    } else {
      return false;
    }
  }
  if (!is_compacting_) return false;
  MarkBit obj_bit = Marking::MarkBitFrom(obj);
  return Marking::IsBlack(obj_bit);
}


void IncrementalMarking::RecordWrite(HeapObject* obj, Object** slot,
                                     Object* value) {
  if (IsMarking() && value->IsHeapObject()) {
    RecordWriteSlow(obj, slot, value);
  }
}


void IncrementalMarking::RecordWriteOfCodeEntry(JSFunction* host, Object** slot,
                                                Code* value) {
  if (IsMarking()) RecordWriteOfCodeEntrySlow(host, slot, value);
}


void IncrementalMarking::RecordWriteIntoCode(HeapObject* obj, RelocInfo* rinfo,
                                             Object* value) {
  if (IsMarking() && value->IsHeapObject()) {
    RecordWriteIntoCodeSlow(obj, rinfo, value);
  }
}


void IncrementalMarking::RecordWrites(HeapObject* obj) {
  if (IsMarking()) {
    MarkBit obj_bit = Marking::MarkBitFrom(obj);
    if (Marking::IsBlack(obj_bit)) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
      if (chunk->IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
        chunk->set_progress_bar(0);
      }
      BlackToGreyAndUnshift(obj, obj_bit);
      RestartIfNotMarking();
    }
  }
}


void IncrementalMarking::BlackToGreyAndUnshift(HeapObject* obj,
                                               MarkBit mark_bit) {
  DCHECK(Marking::MarkBitFrom(obj) == mark_bit);
  DCHECK(obj->Size() >= 2 * kPointerSize);
  DCHECK(IsMarking());
  Marking::BlackToGrey(mark_bit);
  int obj_size = obj->Size();
  MemoryChunk::IncrementLiveBytesFromGC(obj->address(), -obj_size);
  bytes_scanned_ -= obj_size;
  int64_t old_bytes_rescanned = bytes_rescanned_;
  bytes_rescanned_ = old_bytes_rescanned + obj_size;
  if ((bytes_rescanned_ >> 20) != (old_bytes_rescanned >> 20)) {
    if (bytes_rescanned_ > 2 * heap_->PromotedSpaceSizeOfObjects()) {
      // If we have queued twice the heap size for rescanning then we are
      // going around in circles, scanning the same objects again and again
      // as the program mutates the heap faster than we can incrementally
      // trace it.  In this case we switch to non-incremental marking in
      // order to finish off this marking phase.
      if (FLAG_trace_gc) {
        PrintPID("Hurrying incremental marking because of lack of progress\n");
      }
      marking_speed_ = kMaxMarkingSpeed;
    }
  }

  marking_deque_.UnshiftGrey(obj);
}


void IncrementalMarking::WhiteToGreyAndPush(HeapObject* obj, MarkBit mark_bit) {
  Marking::WhiteToGrey(mark_bit);
  marking_deque_.PushGrey(obj);
}
}
}  // namespace v8::internal

#endif  // V8_HEAP_INCREMENTAL_MARKING_INL_H_
