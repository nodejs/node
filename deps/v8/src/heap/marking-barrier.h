// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_BARRIER_H_
#define V8_HEAP_MARKING_BARRIER_H_

#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-worklist.h"

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
  void PublishIfNeeded();

  void ActivateShared();
  void DeactivateShared();
  void PublishSharedIfNeeded();

  static void ActivateAll(Heap* heap, bool is_compacting,
                          MarkingBarrierType marking_barrier_type);
  static void DeactivateAll(Heap* heap);
  V8_EXPORT_PRIVATE static void PublishAll(Heap* heap);

  void Write(HeapObject host, HeapObjectSlot, HeapObject value);
  void Write(InstructionStream host, RelocInfo*, HeapObject value);
  void Write(JSArrayBuffer host, ArrayBufferExtension*);
  void Write(DescriptorArray, int number_of_own_descriptors);
  // Only usable when there's no valid JS host object for this write, e.g., when
  // value is held alive from a global handle.
  void WriteWithoutHost(HeapObject value);

  inline void MarkValue(HeapObject host, HeapObject value);

  bool is_minor() const {
    return marking_barrier_type_ == MarkingBarrierType::kMinor;
  }

  Heap* heap() const { return heap_; }

#if DEBUG
  void AssertMarkingIsActivated() const;
  void AssertSharedMarkingIsActivated() const;
#endif  // DEBUG

 private:
  inline void MarkValueShared(HeapObject value);
  inline void MarkValueLocal(HeapObject value);

  inline bool WhiteToGreyAndPush(HeapObject value);

  void RecordRelocSlot(RelocInfo* rinfo, HeapObject target);

  bool IsCurrentMarkingBarrier(HeapObject verification_candidate);

  template <typename TSlot>
  inline void MarkRange(HeapObject value, TSlot start, TSlot end);

  inline bool IsCompacting(HeapObject object) const;

  bool is_major() const {
    return marking_barrier_type_ == MarkingBarrierType::kMajor;
  }

  Isolate* isolate() const;

  Heap* heap_;
  MarkCompactCollector* major_collector_;
  MinorMarkCompactCollector* minor_collector_;
  IncrementalMarking* incremental_marking_;
  MarkingWorklist::Local major_worklist_;
  MarkingWorklist::Local minor_worklist_;
  MarkingWorklist::Local* current_worklist_;
  base::Optional<MarkingWorklist::Local> shared_heap_worklist_;
  MarkingState marking_state_;
  std::unordered_map<MemoryChunk*, std::unique_ptr<TypedSlots>,
                     MemoryChunk::Hasher>
      typed_slots_map_;
  bool is_compacting_ = false;
  bool is_activated_ = false;
  const bool is_main_thread_barrier_;
  const bool uses_shared_heap_;
  const bool is_shared_space_isolate_;
  MarkingBarrierType marking_barrier_type_ = MarkingBarrierType::kMajor;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_BARRIER_H_
