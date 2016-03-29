// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/slots-buffer.h"

#include "src/assembler.h"
#include "src/heap/heap.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

bool SlotsBuffer::IsTypedSlot(ObjectSlot slot) {
  return reinterpret_cast<uintptr_t>(slot) < NUMBER_OF_SLOT_TYPES;
}


bool SlotsBuffer::AddTo(SlotsBufferAllocator* allocator,
                        SlotsBuffer** buffer_address, SlotType type,
                        Address addr, AdditionMode mode) {
  SlotsBuffer* buffer = *buffer_address;
  if (buffer == NULL || !buffer->HasSpaceForTypedSlot()) {
    if (mode == FAIL_ON_OVERFLOW && ChainLengthThresholdReached(buffer)) {
      allocator->DeallocateChain(buffer_address);
      return false;
    }
    buffer = allocator->AllocateBuffer(buffer);
    *buffer_address = buffer;
  }
  DCHECK(buffer->HasSpaceForTypedSlot());
  buffer->Add(reinterpret_cast<ObjectSlot>(type));
  buffer->Add(reinterpret_cast<ObjectSlot>(addr));
  return true;
}


void SlotsBuffer::RemoveInvalidSlots(Heap* heap, SlotsBuffer* buffer) {
  // Remove entries by replacing them with an old-space slot containing a smi
  // that is located in an unmovable page.
  const ObjectSlot kRemovedEntry = HeapObject::RawField(
      heap->empty_fixed_array(), FixedArrayBase::kLengthOffset);
  DCHECK(Page::FromAddress(reinterpret_cast<Address>(kRemovedEntry))
             ->NeverEvacuate());

  while (buffer != NULL) {
    SlotsBuffer::ObjectSlot* slots = buffer->slots_;
    intptr_t slots_count = buffer->idx_;

    for (int slot_idx = 0; slot_idx < slots_count; ++slot_idx) {
      ObjectSlot slot = slots[slot_idx];
      if (!IsTypedSlot(slot)) {
        Object* object = *slot;
        // Slots are invalid when they currently:
        // - do not point to a heap object (SMI)
        // - point to a heap object in new space
        // - are not within a live heap object on a valid pointer slot
        // - point to a heap object not on an evacuation candidate
        // TODO(mlippautz): Move InNewSpace check above IsSlotInLiveObject once
        //   we filter out unboxed double slots eagerly.
        if (!object->IsHeapObject() ||
            !heap->mark_compact_collector()->IsSlotInLiveObject(
                reinterpret_cast<Address>(slot)) ||
            heap->InNewSpace(object) ||
            !Page::FromAddress(reinterpret_cast<Address>(object))
                 ->IsEvacuationCandidate()) {
          // TODO(hpayer): Instead of replacing slots with kRemovedEntry we
          // could shrink the slots buffer in-place.
          slots[slot_idx] = kRemovedEntry;
        }
      } else {
        ++slot_idx;
        DCHECK(slot_idx < slots_count);
      }
    }
    buffer = buffer->next();
  }
}


void SlotsBuffer::RemoveObjectSlots(Heap* heap, SlotsBuffer* buffer,
                                    Address start_slot, Address end_slot) {
  // Remove entries by replacing them with an old-space slot containing a smi
  // that is located in an unmovable page.
  const ObjectSlot kRemovedEntry = HeapObject::RawField(
      heap->empty_fixed_array(), FixedArrayBase::kLengthOffset);
  DCHECK(Page::FromAddress(reinterpret_cast<Address>(kRemovedEntry))
             ->NeverEvacuate());

  while (buffer != NULL) {
    SlotsBuffer::ObjectSlot* slots = buffer->slots_;
    intptr_t slots_count = buffer->idx_;
    bool is_typed_slot = false;

    for (int slot_idx = 0; slot_idx < slots_count; ++slot_idx) {
      ObjectSlot slot = slots[slot_idx];
      if (!IsTypedSlot(slot)) {
        Address slot_address = reinterpret_cast<Address>(slot);
        if (slot_address >= start_slot && slot_address < end_slot) {
          // TODO(hpayer): Instead of replacing slots with kRemovedEntry we
          // could shrink the slots buffer in-place.
          slots[slot_idx] = kRemovedEntry;
          if (is_typed_slot) {
            slots[slot_idx - 1] = kRemovedEntry;
          }
        }
        is_typed_slot = false;
      } else {
        is_typed_slot = true;
        DCHECK(slot_idx < slots_count);
      }
    }
    buffer = buffer->next();
  }
}


void SlotsBuffer::VerifySlots(Heap* heap, SlotsBuffer* buffer) {
  while (buffer != NULL) {
    SlotsBuffer::ObjectSlot* slots = buffer->slots_;
    intptr_t slots_count = buffer->idx_;

    for (int slot_idx = 0; slot_idx < slots_count; ++slot_idx) {
      ObjectSlot slot = slots[slot_idx];
      if (!IsTypedSlot(slot)) {
        Object* object = *slot;
        if (object->IsHeapObject()) {
          HeapObject* heap_object = HeapObject::cast(object);
          CHECK(!heap->InNewSpace(object));
          heap->mark_compact_collector()->VerifyIsSlotInLiveObject(
              reinterpret_cast<Address>(slot), heap_object);
        }
      } else {
        ++slot_idx;
        DCHECK(slot_idx < slots_count);
      }
    }
    buffer = buffer->next();
  }
}


SlotsBuffer* SlotsBufferAllocator::AllocateBuffer(SlotsBuffer* next_buffer) {
  return new SlotsBuffer(next_buffer);
}


void SlotsBufferAllocator::DeallocateBuffer(SlotsBuffer* buffer) {
  delete buffer;
}


void SlotsBufferAllocator::DeallocateChain(SlotsBuffer** buffer_address) {
  SlotsBuffer* buffer = *buffer_address;
  while (buffer != NULL) {
    SlotsBuffer* next_buffer = buffer->next();
    DeallocateBuffer(buffer);
    buffer = next_buffer;
  }
  *buffer_address = NULL;
}

}  // namespace internal
}  // namespace v8
