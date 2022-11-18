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

enum class MarkingBarrierType { kMinor, kMajor };

class MarkingBarrier {
 public:
  explicit MarkingBarrier(LocalHeap*);
  ~MarkingBarrier();

  void Activate(bool is_compacting, MarkingBarrierType marking_barrier_type);
  void Deactivate();
  void Publish();

  static void ActivateAll(Heap* heap, bool is_compacting,
                          MarkingBarrierType marking_barrier_type);
  static void DeactivateAll(Heap* heap);
  V8_EXPORT_PRIVATE static void PublishAll(Heap* heap);

  void Write(HeapObject host, HeapObjectSlot, HeapObject value);
  void Write(Code host, RelocInfo*, HeapObject value);
  void Write(JSArrayBuffer host, ArrayBufferExtension*);
  void Write(DescriptorArray, int number_of_own_descriptors);
  // Only usable when there's no valid JS host object for this write, e.g., when
  // value is held alive from a global handle.
  void WriteWithoutHost(HeapObject value);

  // Returns true if the slot needs to be recorded.
  inline bool MarkValue(HeapObject host, HeapObject value);

  bool is_minor() const {
    return marking_barrier_type_ == MarkingBarrierType::kMinor;
  }

 private:
  inline bool ShouldMarkObject(HeapObject value) const;
  inline bool WhiteToGreyAndPush(HeapObject value);

  void RecordRelocSlot(Code host, RelocInfo* rinfo, HeapObject target);

  void ActivateSpace(PagedSpace*);
  void ActivateSpace(NewSpace*);

  void DeactivateSpace(PagedSpace*);
  void DeactivateSpace(NewSpace*);

  bool IsCurrentMarkingBarrier();

  template <typename TSlot>
  inline void MarkRange(HeapObject value, TSlot start, TSlot end);

  bool is_major() const {
    return marking_barrier_type_ == MarkingBarrierType::kMajor;
  }

  Heap* heap_;
  MarkCompactCollector* major_collector_;
  MinorMarkCompactCollector* minor_collector_;
  IncrementalMarking* incremental_marking_;
  MarkingWorklist::Local major_worklist_;
  MarkingWorklist::Local minor_worklist_;
  MarkingWorklist::Local* current_worklist_;
  MarkingState marking_state_;
  std::unordered_map<MemoryChunk*, std::unique_ptr<TypedSlots>,
                     MemoryChunk::Hasher>
      typed_slots_map_;
  bool is_compacting_ = false;
  bool is_activated_ = false;
  bool is_main_thread_barrier_;
  const bool uses_shared_heap_;
  const bool is_shared_heap_isolate_;
  MarkingBarrierType marking_barrier_type_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_BARRIER_H_
