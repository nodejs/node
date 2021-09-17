// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_REMEMBERED_SET_H_
#define V8_HEAP_REMEMBERED_SET_H_

#include <memory>

#include "src/base/bounds.h"
#include "src/base/memory.h"
#include "src/codegen/reloc-info.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces.h"
#include "src/heap/worklist.h"

namespace v8 {
namespace internal {

enum RememberedSetIterationMode { SYNCHRONIZED, NON_SYNCHRONIZED };

class RememberedSetOperations {
 public:
  // Given a page and a slot in that page, this function adds the slot to the
  // remembered set.
  template <AccessMode access_mode>
  static void Insert(SlotSet* slot_set, MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    uintptr_t offset = slot_addr - chunk->address();
    slot_set->Insert<access_mode>(offset);
  }

  template <typename Callback>
  static int Iterate(SlotSet* slot_set, MemoryChunk* chunk, Callback callback,
                     SlotSet::EmptyBucketMode mode) {
    int slots = 0;
    if (slot_set != nullptr) {
      slots += slot_set->Iterate(chunk->address(), 0, chunk->buckets(),
                                 callback, mode);
    }
    return slots;
  }

  static void Remove(SlotSet* slot_set, MemoryChunk* chunk, Address slot_addr) {
    if (slot_set != nullptr) {
      uintptr_t offset = slot_addr - chunk->address();
      slot_set->Remove(offset);
    }
  }

  static void RemoveRange(SlotSet* slot_set, MemoryChunk* chunk, Address start,
                          Address end, SlotSet::EmptyBucketMode mode) {
    if (slot_set != nullptr) {
      uintptr_t start_offset = start - chunk->address();
      uintptr_t end_offset = end - chunk->address();
      DCHECK_LT(start_offset, end_offset);
      slot_set->RemoveRange(static_cast<int>(start_offset),
                            static_cast<int>(end_offset), chunk->buckets(),
                            mode);
    }
  }

  static void CheckNoneInRange(SlotSet* slot_set, MemoryChunk* chunk,
                               Address start, Address end) {
    if (slot_set != nullptr) {
      size_t start_bucket = SlotSet::BucketForSlot(start - chunk->address());
      // Both 'end' and 'end_bucket' are exclusive limits, so do some index
      // juggling to make sure we get the right bucket even if the end address
      // is at the start of a bucket.
      size_t end_bucket =
          SlotSet::BucketForSlot(end - chunk->address() - kTaggedSize) + 1;
      slot_set->Iterate(
          chunk->address(), start_bucket, end_bucket,
          [start, end](MaybeObjectSlot slot) {
            CHECK(!base::IsInRange(slot.address(), start, end + 1));
            return KEEP_SLOT;
          },
          SlotSet::KEEP_EMPTY_BUCKETS);
    }
  }
};

template <RememberedSetType type>
class RememberedSet : public AllStatic {
 public:
  // Given a page and a slot in that page, this function adds the slot to the
  // remembered set.
  template <AccessMode access_mode>
  static void Insert(MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    SlotSet* slot_set = chunk->slot_set<type, access_mode>();
    if (slot_set == nullptr) {
      slot_set = chunk->AllocateSlotSet<type>();
    }
    RememberedSetOperations::Insert<access_mode>(slot_set, chunk, slot_addr);
  }

  // Given a page and a slot in that page, this function returns true if
  // the remembered set contains the slot.
  static bool Contains(MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    SlotSet* slot_set = chunk->slot_set<type>();
    if (slot_set == nullptr) {
      return false;
    }
    uintptr_t offset = slot_addr - chunk->address();
    return slot_set->Contains(offset);
  }

  static void CheckNoneInRange(MemoryChunk* chunk, Address start, Address end) {
    SlotSet* slot_set = chunk->slot_set<type>();
    RememberedSetOperations::CheckNoneInRange(slot_set, chunk, start, end);
  }

  // Given a page and a slot in that page, this function removes the slot from
  // the remembered set.
  // If the slot was never added, then the function does nothing.
  static void Remove(MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    SlotSet* slot_set = chunk->slot_set<type>();
    RememberedSetOperations::Remove(slot_set, chunk, slot_addr);
  }

  // Given a page and a range of slots in that page, this function removes the
  // slots from the remembered set.
  static void RemoveRange(MemoryChunk* chunk, Address start, Address end,
                          SlotSet::EmptyBucketMode mode) {
    SlotSet* slot_set = chunk->slot_set<type>();
    RememberedSetOperations::RemoveRange(slot_set, chunk, start, end, mode);
  }

  // Iterates and filters the remembered set with the given callback.
  // The callback should take (Address slot) and return SlotCallbackResult.
  template <typename Callback>
  static void Iterate(Heap* heap, RememberedSetIterationMode mode,
                      Callback callback) {
    IterateMemoryChunks(heap, [mode, callback](MemoryChunk* chunk) {
      if (mode == SYNCHRONIZED) chunk->mutex()->Lock();
      Iterate(chunk, callback);
      if (mode == SYNCHRONIZED) chunk->mutex()->Unlock();
    });
  }

  // Iterates over all memory chunks that contains non-empty slot sets.
  // The callback should take (MemoryChunk* chunk) and return void.
  template <typename Callback>
  static void IterateMemoryChunks(Heap* heap, Callback callback) {
    OldGenerationMemoryChunkIterator it(heap);
    MemoryChunk* chunk;
    while ((chunk = it.next()) != nullptr) {
      SlotSet* slot_set = chunk->slot_set<type>();
      SlotSet* sweeping_slot_set =
          type == OLD_TO_NEW ? chunk->sweeping_slot_set() : nullptr;
      TypedSlotSet* typed_slot_set = chunk->typed_slot_set<type>();
      if (slot_set != nullptr || sweeping_slot_set != nullptr ||
          typed_slot_set != nullptr ||
          chunk->invalidated_slots<type>() != nullptr) {
        callback(chunk);
      }
    }
  }

  // Iterates and filters the remembered set in the given memory chunk with
  // the given callback. The callback should take (Address slot) and return
  // SlotCallbackResult.
  //
  // Notice that |mode| can only be of FREE* or PREFREE* if there are no other
  // threads concurrently inserting slots.
  template <typename Callback>
  static int Iterate(MemoryChunk* chunk, Callback callback,
                     SlotSet::EmptyBucketMode mode) {
    SlotSet* slot_set = chunk->slot_set<type>();
    return RememberedSetOperations::Iterate(slot_set, chunk, callback, mode);
  }

  template <typename Callback>
  static int IterateAndTrackEmptyBuckets(
      MemoryChunk* chunk, Callback callback,
      Worklist<MemoryChunk*, 64>::View empty_chunks) {
    SlotSet* slot_set = chunk->slot_set<type>();
    int slots = 0;
    if (slot_set != nullptr) {
      PossiblyEmptyBuckets* possibly_empty_buckets =
          chunk->possibly_empty_buckets();
      slots += slot_set->IterateAndTrackEmptyBuckets(chunk->address(), 0,
                                                     chunk->buckets(), callback,
                                                     possibly_empty_buckets);
      if (!possibly_empty_buckets->IsEmpty()) empty_chunks.Push(chunk);
    }
    return slots;
  }

  static void FreeEmptyBuckets(MemoryChunk* chunk) {
    DCHECK(type == OLD_TO_NEW);
    SlotSet* slot_set = chunk->slot_set<type>();
    if (slot_set != nullptr && slot_set->FreeEmptyBuckets(chunk->buckets())) {
      chunk->ReleaseSlotSet<type>();
    }
  }

  static bool CheckPossiblyEmptyBuckets(MemoryChunk* chunk) {
    DCHECK(type == OLD_TO_NEW);
    SlotSet* slot_set = chunk->slot_set<type, AccessMode::NON_ATOMIC>();
    if (slot_set != nullptr &&
        slot_set->CheckPossiblyEmptyBuckets(chunk->buckets(),
                                            chunk->possibly_empty_buckets())) {
      chunk->ReleaseSlotSet<type>();
      return true;
    }

    return false;
  }

  // Given a page and a typed slot in that page, this function adds the slot
  // to the remembered set.
  static void InsertTyped(MemoryChunk* memory_chunk, SlotType slot_type,
                          uint32_t offset) {
    TypedSlotSet* slot_set = memory_chunk->typed_slot_set<type>();
    if (slot_set == nullptr) {
      slot_set = memory_chunk->AllocateTypedSlotSet<type>();
    }
    slot_set->Insert(slot_type, offset);
  }

  static void MergeTyped(MemoryChunk* page, std::unique_ptr<TypedSlots> other) {
    TypedSlotSet* slot_set = page->typed_slot_set<type>();
    if (slot_set == nullptr) {
      slot_set = page->AllocateTypedSlotSet<type>();
    }
    slot_set->Merge(other.get());
  }

  // Given a page and a range of typed slots in that page, this function removes
  // the slots from the remembered set.
  static void RemoveRangeTyped(MemoryChunk* page, Address start, Address end) {
    TypedSlotSet* slot_set = page->typed_slot_set<type>();
    if (slot_set != nullptr) {
      slot_set->Iterate(
          [=](SlotType slot_type, Address slot_addr) {
            return start <= slot_addr && slot_addr < end ? REMOVE_SLOT
                                                         : KEEP_SLOT;
          },
          TypedSlotSet::FREE_EMPTY_CHUNKS);
    }
  }

  // Iterates and filters the remembered set with the given callback.
  // The callback should take (SlotType slot_type, Address addr) and return
  // SlotCallbackResult.
  template <typename Callback>
  static void IterateTyped(Heap* heap, RememberedSetIterationMode mode,
                           Callback callback) {
    IterateMemoryChunks(heap, [mode, callback](MemoryChunk* chunk) {
      if (mode == SYNCHRONIZED) chunk->mutex()->Lock();
      IterateTyped(chunk, callback);
      if (mode == SYNCHRONIZED) chunk->mutex()->Unlock();
    });
  }

  // Iterates and filters typed pointers in the given memory chunk with the
  // given callback. The callback should take (SlotType slot_type, Address addr)
  // and return SlotCallbackResult.
  template <typename Callback>
  static void IterateTyped(MemoryChunk* chunk, Callback callback) {
    TypedSlotSet* slot_set = chunk->typed_slot_set<type>();
    if (slot_set != nullptr) {
      int new_count =
          slot_set->Iterate(callback, TypedSlotSet::KEEP_EMPTY_CHUNKS);
      if (new_count == 0) {
        chunk->ReleaseTypedSlotSet<type>();
      }
    }
  }

  // Clear all old to old slots from the remembered set.
  static void ClearAll(Heap* heap) {
    STATIC_ASSERT(type == OLD_TO_OLD || type == OLD_TO_CODE);
    OldGenerationMemoryChunkIterator it(heap);
    MemoryChunk* chunk;
    while ((chunk = it.next()) != nullptr) {
      chunk->ReleaseSlotSet<OLD_TO_OLD>();
      if (V8_EXTERNAL_CODE_SPACE_BOOL) {
        chunk->ReleaseSlotSet<OLD_TO_CODE>();
      }
      chunk->ReleaseTypedSlotSet<OLD_TO_OLD>();
      chunk->ReleaseInvalidatedSlots<OLD_TO_OLD>();
    }
  }
};

class UpdateTypedSlotHelper {
 public:
  // Updates a typed slot using an untyped slot callback where |addr| depending
  // on slot type represents either address for respective RelocInfo or address
  // of the uncompressed constant pool entry.
  // The callback accepts FullMaybeObjectSlot and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateTypedSlot(Heap* heap, SlotType slot_type,
                                            Address addr, Callback callback);

 private:
  // Updates a code entry slot using an untyped slot callback.
  // The callback accepts FullMaybeObjectSlot and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateCodeEntry(Address entry_address,
                                            Callback callback) {
    Code code = Code::GetObjectFromEntryAddress(entry_address);
    Code old_code = code;
    SlotCallbackResult result = callback(FullMaybeObjectSlot(&code));
    DCHECK(!HasWeakHeapObjectTag(code));
    if (code != old_code) {
      base::Memory<Address>(entry_address) = code.entry();
    }
    return result;
  }

  // Updates a code target slot using an untyped slot callback.
  // The callback accepts FullMaybeObjectSlot and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateCodeTarget(RelocInfo* rinfo,
                                             Callback callback) {
    DCHECK(RelocInfo::IsCodeTargetMode(rinfo->rmode()));
    Code old_target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    Code new_target = old_target;
    SlotCallbackResult result = callback(FullMaybeObjectSlot(&new_target));
    DCHECK(!HasWeakHeapObjectTag(new_target));
    if (new_target != old_target) {
      rinfo->set_target_address(Code::cast(new_target).raw_instruction_start());
    }
    return result;
  }

  // Updates an embedded pointer slot using an untyped slot callback.
  // The callback accepts FullMaybeObjectSlot and returns SlotCallbackResult.
  template <typename Callback>
  static SlotCallbackResult UpdateEmbeddedPointer(Heap* heap, RelocInfo* rinfo,
                                                  Callback callback) {
    DCHECK(RelocInfo::IsEmbeddedObjectMode(rinfo->rmode()));
    HeapObject old_target = rinfo->target_object_no_host(heap->isolate());
    HeapObject new_target = old_target;
    SlotCallbackResult result = callback(FullMaybeObjectSlot(&new_target));
    DCHECK(!HasWeakHeapObjectTag(new_target));
    if (new_target != old_target) {
      rinfo->set_target_object(heap, HeapObject::cast(new_target));
    }
    return result;
  }
};

class RememberedSetSweeping {
 public:
  template <AccessMode access_mode>
  static void Insert(MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    SlotSet* slot_set = chunk->sweeping_slot_set<access_mode>();
    if (slot_set == nullptr) {
      slot_set = chunk->AllocateSweepingSlotSet();
    }
    RememberedSetOperations::Insert<access_mode>(slot_set, chunk, slot_addr);
  }

  static void Remove(MemoryChunk* chunk, Address slot_addr) {
    DCHECK(chunk->Contains(slot_addr));
    SlotSet* slot_set = chunk->sweeping_slot_set<AccessMode::ATOMIC>();
    RememberedSetOperations::Remove(slot_set, chunk, slot_addr);
  }

  // Given a page and a range of slots in that page, this function removes the
  // slots from the remembered set.
  static void RemoveRange(MemoryChunk* chunk, Address start, Address end,
                          SlotSet::EmptyBucketMode mode) {
    SlotSet* slot_set = chunk->sweeping_slot_set();
    RememberedSetOperations::RemoveRange(slot_set, chunk, start, end, mode);
  }

  // Iterates and filters the remembered set in the given memory chunk with
  // the given callback. The callback should take (Address slot) and return
  // SlotCallbackResult.
  //
  // Notice that |mode| can only be of FREE* or PREFREE* if there are no other
  // threads concurrently inserting slots.
  template <typename Callback>
  static int Iterate(MemoryChunk* chunk, Callback callback,
                     SlotSet::EmptyBucketMode mode) {
    SlotSet* slot_set = chunk->sweeping_slot_set();
    return RememberedSetOperations::Iterate(slot_set, chunk, callback, mode);
  }
};

inline SlotType SlotTypeForRelocInfoMode(RelocInfo::Mode rmode) {
  if (RelocInfo::IsCodeTargetMode(rmode)) {
    return CODE_TARGET_SLOT;
  } else if (RelocInfo::IsFullEmbeddedObject(rmode)) {
    return FULL_EMBEDDED_OBJECT_SLOT;
  } else if (RelocInfo::IsCompressedEmbeddedObject(rmode)) {
    return COMPRESSED_EMBEDDED_OBJECT_SLOT;
  } else if (RelocInfo::IsDataEmbeddedObject(rmode)) {
    return DATA_EMBEDDED_OBJECT_SLOT;
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_REMEMBERED_SET_H_
