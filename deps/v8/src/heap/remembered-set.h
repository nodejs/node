// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REMEMBERED_SET_H
#define V8_REMEMBERED_SET_H

#include "src/assembler.h"
#include "src/heap/heap.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

enum PointerDirection { OLD_TO_OLD, OLD_TO_NEW };

// TODO(ulan): Investigate performance of de-templatizing this class.
template <PointerDirection direction>
class RememberedSet {
 public:
  // Given a page and a slot in that page, this function adds the slot to the
  // remembered set.
  static void Insert(MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    SlotSet* slot_set = GetSlotSet(chunk);
    if (slot_set == nullptr) {
      slot_set = AllocateSlotSet(chunk);
    }
    uintptr_t offset = slot_addr - chunk->address();
    slot_set[offset / Page::kPageSize].Insert(offset % Page::kPageSize);
  }

  // Given a page and a slot in that page, this function removes the slot from
  // the remembered set.
  // If the slot was never added, then the function does nothing.
  static void Remove(MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    SlotSet* slot_set = GetSlotSet(chunk);
    if (slot_set != nullptr) {
      uintptr_t offset = slot_addr - chunk->address();
      slot_set[offset / Page::kPageSize].Remove(offset % Page::kPageSize);
    }
  }

  // Given a page and a range of slots in that page, this function removes the
  // slots from the remembered set.
  static void RemoveRange(MemoryChunk* chunk, Address start, Address end) {
    SlotSet* slot_set = GetSlotSet(chunk);
    if (slot_set != nullptr) {
      uintptr_t start_offset = start - chunk->address();
      uintptr_t end_offset = end - chunk->address();
      DCHECK_LT(start_offset, end_offset);
      if (end_offset < static_cast<uintptr_t>(Page::kPageSize)) {
        slot_set->RemoveRange(static_cast<int>(start_offset),
                              static_cast<int>(end_offset));
      } else {
        // The large page has multiple slot sets.
        // Compute slot set indicies for the range [start_offset, end_offset).
        int start_chunk = static_cast<int>(start_offset / Page::kPageSize);
        int end_chunk = static_cast<int>((end_offset - 1) / Page::kPageSize);
        int offset_in_start_chunk =
            static_cast<int>(start_offset % Page::kPageSize);
        // Note that using end_offset % Page::kPageSize would be incorrect
        // because end_offset is one beyond the last slot to clear.
        int offset_in_end_chunk = static_cast<int>(
            end_offset - static_cast<uintptr_t>(end_chunk) * Page::kPageSize);
        if (start_chunk == end_chunk) {
          slot_set[start_chunk].RemoveRange(offset_in_start_chunk,
                                            offset_in_end_chunk);
        } else {
          // Clear all slots from start_offset to the end of first chunk.
          slot_set[start_chunk].RemoveRange(offset_in_start_chunk,
                                            Page::kPageSize);
          // Clear all slots in intermediate chunks.
          for (int i = start_chunk + 1; i < end_chunk; i++) {
            slot_set[i].RemoveRange(0, Page::kPageSize);
          }
          // Clear slots from the beginning of the last page to end_offset.
          slot_set[end_chunk].RemoveRange(0, offset_in_end_chunk);
        }
      }
    }
  }

  // Iterates and filters the remembered set with the given callback.
  // The callback should take (Address slot) and return SlotCallbackResult.
  template <typename Callback>
  static void Iterate(Heap* heap, Callback callback) {
    IterateMemoryChunks(
        heap, [callback](MemoryChunk* chunk) { Iterate(chunk, callback); });
  }

  // Iterates over all memory chunks that contains non-empty slot sets.
  // The callback should take (MemoryChunk* chunk) and return void.
  template <typename Callback>
  static void IterateMemoryChunks(Heap* heap, Callback callback) {
    MemoryChunkIterator it(heap);
    MemoryChunk* chunk;
    while ((chunk = it.next()) != nullptr) {
      SlotSet* slots = GetSlotSet(chunk);
      TypedSlotSet* typed_slots = GetTypedSlotSet(chunk);
      if (slots != nullptr || typed_slots != nullptr) {
        callback(chunk);
      }
    }
  }

  // Iterates and filters the remembered set in the given memory chunk with
  // the given callback. The callback should take (Address slot) and return
  // SlotCallbackResult.
  template <typename Callback>
  static void Iterate(MemoryChunk* chunk, Callback callback) {
    SlotSet* slots = GetSlotSet(chunk);
    if (slots != nullptr) {
      size_t pages = (chunk->size() + Page::kPageSize - 1) / Page::kPageSize;
      int new_count = 0;
      for (size_t page = 0; page < pages; page++) {
        new_count +=
            slots[page].Iterate(callback, SlotSet::PREFREE_EMPTY_BUCKETS);
      }
      // Only old-to-old slot sets are released eagerly. Old-new-slot sets are
      // released by the sweeper threads.
      if (direction == OLD_TO_OLD && new_count == 0) {
        chunk->ReleaseOldToOldSlots();
      }
    }
  }

  // Given a page and a typed slot in that page, this function adds the slot
  // to the remembered set.
  static void InsertTyped(Page* page, Address host_addr, SlotType slot_type,
                          Address slot_addr) {
    TypedSlotSet* slot_set = GetTypedSlotSet(page);
    if (slot_set == nullptr) {
      AllocateTypedSlotSet(page);
      slot_set = GetTypedSlotSet(page);
    }
    if (host_addr == nullptr) {
      host_addr = page->address();
    }
    uintptr_t offset = slot_addr - page->address();
    uintptr_t host_offset = host_addr - page->address();
    DCHECK_LT(offset, static_cast<uintptr_t>(TypedSlotSet::kMaxOffset));
    DCHECK_LT(host_offset, static_cast<uintptr_t>(TypedSlotSet::kMaxOffset));
    slot_set->Insert(slot_type, static_cast<uint32_t>(host_offset),
                     static_cast<uint32_t>(offset));
  }

  // Given a page and a range of typed slots in that page, this function removes
  // the slots from the remembered set.
  static void RemoveRangeTyped(MemoryChunk* page, Address start, Address end) {
    TypedSlotSet* slots = GetTypedSlotSet(page);
    if (slots != nullptr) {
      slots->Iterate(
          [start, end](SlotType slot_type, Address host_addr,
                       Address slot_addr) {
            return start <= slot_addr && slot_addr < end ? REMOVE_SLOT
                                                         : KEEP_SLOT;
          },
          TypedSlotSet::PREFREE_EMPTY_CHUNKS);
    }
  }

  // Iterates and filters the remembered set with the given callback.
  // The callback should take (SlotType slot_type, SlotAddress slot) and return
  // SlotCallbackResult.
  template <typename Callback>
  static void IterateTyped(Heap* heap, Callback callback) {
    IterateMemoryChunks(heap, [callback](MemoryChunk* chunk) {
      IterateTyped(chunk, callback);
    });
  }

  // Iterates and filters typed old to old pointers in the given memory chunk
  // with the given callback. The callback should take (SlotType slot_type,
  // Address slot_addr) and return SlotCallbackResult.
  template <typename Callback>
  static void IterateTyped(MemoryChunk* chunk, Callback callback) {
    TypedSlotSet* slots = GetTypedSlotSet(chunk);
    if (slots != nullptr) {
      int new_count = slots->Iterate(callback, TypedSlotSet::KEEP_EMPTY_CHUNKS);
      if (new_count == 0) {
        ReleaseTypedSlotSet(chunk);
      }
    }
  }

  // Clear all old to old slots from the remembered set.
  static void ClearAll(Heap* heap) {
    STATIC_ASSERT(direction == OLD_TO_OLD);
    MemoryChunkIterator it(heap);
    MemoryChunk* chunk;
    while ((chunk = it.next()) != nullptr) {
      chunk->ReleaseOldToOldSlots();
      chunk->ReleaseTypedOldToOldSlots();
    }
  }

  // Eliminates all stale slots from the remembered set, i.e.
  // slots that are not part of live objects anymore. This method must be
  // called after marking, when the whole transitive closure is known and
  // must be called before sweeping when mark bits are still intact.
  static void ClearInvalidSlots(Heap* heap);

  static void VerifyValidSlots(Heap* heap);

 private:
  static SlotSet* GetSlotSet(MemoryChunk* chunk) {
    if (direction == OLD_TO_OLD) {
      return chunk->old_to_old_slots();
    } else {
      return chunk->old_to_new_slots();
    }
  }

  static TypedSlotSet* GetTypedSlotSet(MemoryChunk* chunk) {
    if (direction == OLD_TO_OLD) {
      return chunk->typed_old_to_old_slots();
    } else {
      return chunk->typed_old_to_new_slots();
    }
  }

  static void ReleaseTypedSlotSet(MemoryChunk* chunk) {
    if (direction == OLD_TO_OLD) {
      chunk->ReleaseTypedOldToOldSlots();
    }
  }

  static SlotSet* AllocateSlotSet(MemoryChunk* chunk) {
    if (direction == OLD_TO_OLD) {
      chunk->AllocateOldToOldSlots();
      return chunk->old_to_old_slots();
    } else {
      chunk->AllocateOldToNewSlots();
      return chunk->old_to_new_slots();
    }
  }

  static TypedSlotSet* AllocateTypedSlotSet(MemoryChunk* chunk) {
    if (direction == OLD_TO_OLD) {
      chunk->AllocateTypedOldToOldSlots();
      return chunk->typed_old_to_old_slots();
    } else {
      chunk->AllocateTypedOldToNewSlots();
      return chunk->typed_old_to_new_slots();
    }
  }

  static bool IsValidSlot(Heap* heap, MemoryChunk* chunk, Object** slot);
};

class UpdateTypedSlotHelper {
 public:
  // Updates a cell slot using an untyped slot callback.
  // The callback accepts (Heap*, Object**) and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateCell(RelocInfo* rinfo, Callback callback) {
    DCHECK(rinfo->rmode() == RelocInfo::CELL);
    Object* cell = rinfo->target_cell();
    Object* old_cell = cell;
    SlotCallbackResult result = callback(&cell);
    if (cell != old_cell) {
      rinfo->set_target_cell(reinterpret_cast<Cell*>(cell));
    }
    return result;
  }

  // Updates a code entry slot using an untyped slot callback.
  // The callback accepts (Heap*, Object**) and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateCodeEntry(Address entry_address,
                                            Callback callback) {
    Object* code = Code::GetObjectFromEntryAddress(entry_address);
    Object* old_code = code;
    SlotCallbackResult result = callback(&code);
    if (code != old_code) {
      Memory::Address_at(entry_address) =
          reinterpret_cast<Code*>(code)->entry();
    }
    return result;
  }

  // Updates a code target slot using an untyped slot callback.
  // The callback accepts (Heap*, Object**) and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateCodeTarget(RelocInfo* rinfo,
                                             Callback callback) {
    DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    Object* old_target = target;
    SlotCallbackResult result = callback(&target);
    if (target != old_target) {
      rinfo->set_target_address(Code::cast(target)->instruction_start());
    }
    return result;
  }

  // Updates an embedded pointer slot using an untyped slot callback.
  // The callback accepts (Heap*, Object**) and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateEmbeddedPointer(RelocInfo* rinfo,
                                                  Callback callback) {
    DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
    Object* target = rinfo->target_object();
    Object* old_target = target;
    SlotCallbackResult result = callback(&target);
    if (target != old_target) {
      rinfo->set_target_object(target);
    }
    return result;
  }

  // Updates a debug target slot using an untyped slot callback.
  // The callback accepts (Heap*, Object**) and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateDebugTarget(RelocInfo* rinfo,
                                              Callback callback) {
    DCHECK(RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
           rinfo->IsPatchedDebugBreakSlotSequence());
    Object* target =
        Code::GetCodeFromTargetAddress(rinfo->debug_call_address());
    SlotCallbackResult result = callback(&target);
    rinfo->set_debug_call_address(Code::cast(target)->instruction_start());
    return result;
  }

  // Updates a typed slot using an untyped slot callback.
  // The callback accepts (Heap*, Object**) and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateTypedSlot(Isolate* isolate,
                                            SlotType slot_type, Address addr,
                                            Callback callback) {
    switch (slot_type) {
      case CODE_TARGET_SLOT: {
        RelocInfo rinfo(isolate, addr, RelocInfo::CODE_TARGET, 0, NULL);
        return UpdateCodeTarget(&rinfo, callback);
      }
      case CELL_TARGET_SLOT: {
        RelocInfo rinfo(isolate, addr, RelocInfo::CELL, 0, NULL);
        return UpdateCell(&rinfo, callback);
      }
      case CODE_ENTRY_SLOT: {
        return UpdateCodeEntry(addr, callback);
      }
      case DEBUG_TARGET_SLOT: {
        RelocInfo rinfo(isolate, addr, RelocInfo::DEBUG_BREAK_SLOT_AT_POSITION,
                        0, NULL);
        if (rinfo.IsPatchedDebugBreakSlotSequence()) {
          return UpdateDebugTarget(&rinfo, callback);
        }
        return REMOVE_SLOT;
      }
      case EMBEDDED_OBJECT_SLOT: {
        RelocInfo rinfo(isolate, addr, RelocInfo::EMBEDDED_OBJECT, 0, NULL);
        return UpdateEmbeddedPointer(&rinfo, callback);
      }
      case OBJECT_SLOT: {
        return callback(reinterpret_cast<Object**>(addr));
      }
      case CLEARED_SLOT:
        break;
    }
    UNREACHABLE();
    return REMOVE_SLOT;
  }
};

inline SlotType SlotTypeForRelocInfoMode(RelocInfo::Mode rmode) {
  if (RelocInfo::IsCodeTarget(rmode)) {
    return CODE_TARGET_SLOT;
  } else if (RelocInfo::IsCell(rmode)) {
    return CELL_TARGET_SLOT;
  } else if (RelocInfo::IsEmbeddedObject(rmode)) {
    return EMBEDDED_OBJECT_SLOT;
  } else if (RelocInfo::IsDebugBreakSlot(rmode)) {
    return DEBUG_TARGET_SLOT;
  }
  UNREACHABLE();
  return CLEARED_SLOT;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_REMEMBERED_SET_H
