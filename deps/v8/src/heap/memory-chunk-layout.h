// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_LAYOUT_H_
#define V8_HEAP_MEMORY_CHUNK_LAYOUT_H_

#include "src/heap/base/active-system-pages.h"
#include "src/heap/heap.h"
#include "src/heap/list.h"
#include "src/heap/progress-bar.h"
#include "src/heap/slot-set.h"

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
#include "src/heap/object-start-bitmap.h"
#endif

namespace v8 {
namespace internal {

class Bitmap;
class CodeObjectRegistry;
class FreeListCategory;
class Heap;
class TypedSlotsSet;
class SlotSet;

enum RememberedSetType {
  OLD_TO_NEW,
  OLD_TO_OLD,
  OLD_TO_SHARED,
  OLD_TO_CODE = V8_EXTERNAL_CODE_SPACE_BOOL ? OLD_TO_SHARED + 1 : OLD_TO_SHARED,
  NUMBER_OF_REMEMBERED_SET_TYPES
};

using ActiveSystemPages = ::heap::base::ActiveSystemPages;

class V8_EXPORT_PRIVATE MemoryChunkLayout {
 public:
  static const int kNumSets = NUMBER_OF_REMEMBERED_SET_TYPES;
  static const int kNumTypes = ExternalBackingStoreType::kNumTypes;
#define FIELD(Type, Name) \
  k##Name##Offset, k##Name##End = k##Name##Offset + sizeof(Type) - 1
  enum Header {
    // BasicMemoryChunk fields:
    FIELD(size_t, Size),
    FIELD(uintptr_t, Flags),
    FIELD(Heap*, Heap),
    FIELD(Address, AreaStart),
    FIELD(Address, AreaEnd),
    FIELD(size_t, AllocatedBytes),
    FIELD(size_t, WastedMemory),
    FIELD(std::atomic<intptr_t>, HighWaterMark),
    FIELD(Address, Owner),
    FIELD(VirtualMemory, Reservation),
    // MemoryChunk fields:
    FIELD(SlotSet* [kNumSets], SlotSet),
    FIELD(ProgressBar, ProgressBar),
    FIELD(std::atomic<intptr_t>, LiveByteCount),
    FIELD(TypedSlotsSet* [kNumSets], TypedSlotSet),
    FIELD(void* [kNumSets], InvalidatedSlots),
    FIELD(base::Mutex*, Mutex),
    FIELD(std::atomic<intptr_t>, ConcurrentSweeping),
    FIELD(base::Mutex*, PageProtectionChangeMutex),
    FIELD(uintptr_t, WriteUnprotectCounter),
    FIELD(std::atomic<size_t>[kNumTypes], ExternalBackingStoreBytes),
    FIELD(heap::ListNode<MemoryChunk>, ListNode),
    FIELD(FreeListCategory**, Categories),
    FIELD(std::atomic<intptr_t>, YoungGenerationLiveByteCount),
    FIELD(Bitmap*, YoungGenerationBitmap),
    FIELD(CodeObjectRegistry*, CodeObjectRegistry),
    FIELD(PossiblyEmptyBuckets, PossiblyEmptyBuckets),
    FIELD(ActiveSystemPages, ActiveSystemPages),
#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
    FIELD(ObjectStartBitmap, ObjectStartBitmap),
#endif
    kMarkingBitmapOffset,
    kMemoryChunkHeaderSize = kMarkingBitmapOffset,
    kMemoryChunkHeaderStart = kSlotSetOffset,
    kBasicMemoryChunkHeaderSize = kMemoryChunkHeaderStart,
    kBasicMemoryChunkHeaderStart = 0,
  };
  static size_t CodePageGuardStartOffset();
  static size_t CodePageGuardSize();
  static intptr_t ObjectStartOffsetInCodePage();
  static intptr_t ObjectEndOffsetInCodePage();
  static size_t AllocatableMemoryInCodePage();
  static intptr_t ObjectStartOffsetInDataPage();
  static size_t AllocatableMemoryInDataPage();
  static size_t ObjectStartOffsetInMemoryChunk(AllocationSpace space);
  static size_t AllocatableMemoryInMemoryChunk(AllocationSpace space);

  static int MaxRegularCodeObjectSize();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_CHUNK_LAYOUT_H_
