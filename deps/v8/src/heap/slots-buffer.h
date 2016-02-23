// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SLOTS_BUFFER_H_
#define V8_HEAP_SLOTS_BUFFER_H_

#include "src/objects.h"

namespace v8 {
namespace internal {

// Forward declarations.
class SlotsBuffer;


// SlotsBufferAllocator manages the allocation and deallocation of slots buffer
// chunks and links them together. Slots buffer chunks are always created by the
// SlotsBufferAllocator.
class SlotsBufferAllocator {
 public:
  SlotsBuffer* AllocateBuffer(SlotsBuffer* next_buffer);
  void DeallocateBuffer(SlotsBuffer* buffer);

  void DeallocateChain(SlotsBuffer** buffer_address);
};


// SlotsBuffer records a sequence of slots that has to be updated
// after live objects were relocated from evacuation candidates.
// All slots are either untyped or typed:
//    - Untyped slots are expected to contain a tagged object pointer.
//      They are recorded by an address.
//    - Typed slots are expected to contain an encoded pointer to a heap
//      object where the way of encoding depends on the type of the slot.
//      They are recorded as a pair (SlotType, slot address).
// We assume that zero-page is never mapped this allows us to distinguish
// untyped slots from typed slots during iteration by a simple comparison:
// if element of slots buffer is less than NUMBER_OF_SLOT_TYPES then it
// is the first element of typed slot's pair.
class SlotsBuffer {
 public:
  typedef Object** ObjectSlot;

  explicit SlotsBuffer(SlotsBuffer* next_buffer)
      : idx_(0), chain_length_(1), next_(next_buffer) {
    if (next_ != NULL) {
      chain_length_ = next_->chain_length_ + 1;
    }
  }

  ~SlotsBuffer() {}

  void Add(ObjectSlot slot) {
    DCHECK(0 <= idx_ && idx_ < kNumberOfElements);
#ifdef DEBUG
    if (slot >= reinterpret_cast<ObjectSlot>(NUMBER_OF_SLOT_TYPES)) {
      DCHECK_NOT_NULL(*slot);
    }
#endif
    slots_[idx_++] = slot;
  }

  ObjectSlot Get(intptr_t i) {
    DCHECK(i >= 0 && i < kNumberOfElements);
    return slots_[i];
  }

  size_t Size() {
    DCHECK(idx_ <= kNumberOfElements);
    return idx_;
  }

  enum SlotType {
    EMBEDDED_OBJECT_SLOT,
    OBJECT_SLOT,
    RELOCATED_CODE_OBJECT,
    CELL_TARGET_SLOT,
    CODE_TARGET_SLOT,
    CODE_ENTRY_SLOT,
    DEBUG_TARGET_SLOT,
    NUMBER_OF_SLOT_TYPES
  };

  static const char* SlotTypeToString(SlotType type) {
    switch (type) {
      case EMBEDDED_OBJECT_SLOT:
        return "EMBEDDED_OBJECT_SLOT";
      case OBJECT_SLOT:
        return "OBJECT_SLOT";
      case RELOCATED_CODE_OBJECT:
        return "RELOCATED_CODE_OBJECT";
      case CELL_TARGET_SLOT:
        return "CELL_TARGET_SLOT";
      case CODE_TARGET_SLOT:
        return "CODE_TARGET_SLOT";
      case CODE_ENTRY_SLOT:
        return "CODE_ENTRY_SLOT";
      case DEBUG_TARGET_SLOT:
        return "DEBUG_TARGET_SLOT";
      case NUMBER_OF_SLOT_TYPES:
        return "NUMBER_OF_SLOT_TYPES";
    }
    return "UNKNOWN SlotType";
  }

  SlotsBuffer* next() { return next_; }

  static int SizeOfChain(SlotsBuffer* buffer) {
    if (buffer == NULL) return 0;
    return static_cast<int>(buffer->idx_ +
                            (buffer->chain_length_ - 1) * kNumberOfElements);
  }

  inline bool IsFull() { return idx_ == kNumberOfElements; }

  inline bool HasSpaceForTypedSlot() { return idx_ < kNumberOfElements - 1; }

  enum AdditionMode { FAIL_ON_OVERFLOW, IGNORE_OVERFLOW };

  static bool ChainLengthThresholdReached(SlotsBuffer* buffer) {
    return buffer != NULL && buffer->chain_length_ >= kChainLengthThreshold;
  }

  INLINE(static bool AddTo(SlotsBufferAllocator* allocator,
                           SlotsBuffer** buffer_address, ObjectSlot slot,
                           AdditionMode mode)) {
    SlotsBuffer* buffer = *buffer_address;
    if (buffer == NULL || buffer->IsFull()) {
      if (mode == FAIL_ON_OVERFLOW && ChainLengthThresholdReached(buffer)) {
        allocator->DeallocateChain(buffer_address);
        return false;
      }
      buffer = allocator->AllocateBuffer(buffer);
      *buffer_address = buffer;
    }
    buffer->Add(slot);
    return true;
  }

  static bool IsTypedSlot(ObjectSlot slot);

  static bool AddTo(SlotsBufferAllocator* allocator,
                    SlotsBuffer** buffer_address, SlotType type, Address addr,
                    AdditionMode mode);

  // Eliminates all stale entries from the slots buffer, i.e., slots that
  // are not part of live objects anymore. This method must be called after
  // marking, when the whole transitive closure is known and must be called
  // before sweeping when mark bits are still intact.
  static void RemoveInvalidSlots(Heap* heap, SlotsBuffer* buffer);

  // Eliminate all slots that are within the given address range.
  static void RemoveObjectSlots(Heap* heap, SlotsBuffer* buffer,
                                Address start_slot, Address end_slot);

  // Ensures that there are no invalid slots in the chain of slots buffers.
  static void VerifySlots(Heap* heap, SlotsBuffer* buffer);

  static const int kNumberOfElements = 1021;

 private:
  static const int kChainLengthThreshold = 15;

  intptr_t idx_;
  intptr_t chain_length_;
  SlotsBuffer* next_;
  ObjectSlot slots_[kNumberOfElements];
};


}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SLOTS_BUFFER_H_
