// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OFF_THREAD_HEAP_H_
#define V8_HEAP_OFF_THREAD_HEAP_H_

#include <vector>
#include "src/common/globals.h"
#include "src/heap/large-spaces.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE OffThreadHeap {
 public:
  explicit OffThreadHeap(Heap* heap);

  HeapObject AllocateRaw(int size, AllocationType allocation,
                         AllocationAlignment alignment = kWordAligned);
  void AddToScriptList(Handle<Script> shared);

  HeapObject CreateFillerObjectAt(Address addr, int size,
                                  ClearFreedMemoryMode clear_memory_mode);

  void FinishOffThread();
  void Publish(Heap* heap);

 private:
  class StringSlotCollectingVisitor;

  struct RelativeSlot {
    RelativeSlot() = default;
    RelativeSlot(Address object_address, int slot_offset)
        : object_address(object_address), slot_offset(slot_offset) {}

    Address object_address;
    int slot_offset;
  };

  OffThreadSpace space_;
  OffThreadLargeObjectSpace lo_space_;
  std::vector<RelativeSlot> string_slots_;
  std::vector<Script> script_list_;
  bool is_finished = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OFF_THREAD_HEAP_H_
