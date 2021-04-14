// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_BARRIER_H_
#define V8_HEAP_MARKING_BARRIER_H_

#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/heap/mark-compact.h"

namespace v8 {
namespace internal {

class Heap;
class IncrementalMarking;
class LocalHeap;
class PagedSpace;
class NewSpace;

class MarkingBarrier {
 public:
  explicit MarkingBarrier(Heap*);
  explicit MarkingBarrier(LocalHeap*);
  ~MarkingBarrier();

  void Activate(bool is_compacting);
  void Deactivate();
  void Publish();

  static void ActivateAll(Heap* heap, bool is_compacting);
  static void DeactivateAll(Heap* heap);
  static void PublishAll(Heap* heap);

  void Write(HeapObject host, HeapObjectSlot, HeapObject value);
  void Write(Code host, RelocInfo*, HeapObject value);
  void Write(JSArrayBuffer host, ArrayBufferExtension*);
  void Write(DescriptorArray, int number_of_own_descriptors);

  // Returns true if the slot needs to be recorded.
  inline bool MarkValue(HeapObject host, HeapObject value);

 private:
  using MarkingState = MarkCompactCollector::MarkingState;

  inline bool WhiteToGreyAndPush(HeapObject value);

  void RecordRelocSlot(Code host, RelocInfo* rinfo, HeapObject target);

  void ActivateSpace(PagedSpace*);
  void ActivateSpace(NewSpace*);

  void DeactivateSpace(PagedSpace*);
  void DeactivateSpace(NewSpace*);

  bool IsCurrentMarkingBarrier();

  Heap* heap_;
  MarkCompactCollector* collector_;
  IncrementalMarking* incremental_marking_;
  MarkingWorklist::Local worklist_;
  MarkingState marking_state_;
  std::unordered_map<MemoryChunk*, std::unique_ptr<TypedSlots>,
                     MemoryChunk::Hasher>
      typed_slots_map_;
  bool is_compacting_ = false;
  bool is_activated_ = false;
  bool is_main_thread_barrier_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_BARRIER_H_
