// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_LAYOUT_H_
#define V8_HEAP_MEMORY_CHUNK_LAYOUT_H_

#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/base/active-system-pages.h"
#include "src/heap/list.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/progress-bar.h"
#include "src/heap/slot-set.h"
#include "src/objects/instruction-stream.h"

namespace v8::internal {

class FreeListCategory;
class Heap;
class MarkingBitmap;
class MemoryChunkMetadata;
class SlotSet;
class TypedSlotsSet;

enum RememberedSetType {
  OLD_TO_NEW,
  OLD_TO_NEW_BACKGROUND,
  OLD_TO_OLD,
  OLD_TO_SHARED,
  OLD_TO_CODE,
  TRUSTED_TO_TRUSTED,
  TRUSTED_TO_SHARED_TRUSTED,
  SURVIVOR_TO_EXTERNAL_POINTER,
  NUMBER_OF_REMEMBERED_SET_TYPES
};

using ActiveSystemPages = ::heap::base::ActiveSystemPages;

class V8_EXPORT_PRIVATE MemoryChunkLayout {
 public:
  static constexpr int kNumSets = NUMBER_OF_REMEMBERED_SET_TYPES;
  static constexpr int kNumTypes =
      static_cast<int>(ExternalBackingStoreType::kNumValues);
  static constexpr int kMemoryChunkAlignment = sizeof(size_t);
#define FIELD(Type, Name) \
  k##Name##Offset, k##Name##End = k##Name##Offset + sizeof(Type) - 1
  enum Header {
    FIELD(uintptr_t, Flags),
#ifdef V8_ENABLE_SANDBOX
    FIELD(uint32_t, MetadataIndex),
#else
    FIELD(MemoryChunkMetadata*, Metadata),
#endif
    kEndOfMetadataPointer,
    kMemoryChunkHeaderSize =
        kEndOfMetadataPointer +
        ((kEndOfMetadataPointer % kMemoryChunkAlignment) == 0
             ? 0
             : kMemoryChunkAlignment -
                   (kEndOfMetadataPointer % kMemoryChunkAlignment)),
  };
  // Note that the order of the fields is performance sensitive. Often accessed
  // fields should be on the same cache line.
  enum Metadata {
    // MemoryChunkMetadata fields:
    FIELD(VirtualMemory, Reservation),
    FIELD(size_t, AllocatedBytes),
    FIELD(size_t, WastedMemory),
    FIELD(std::atomic<intptr_t>, HighWaterMark),
    FIELD(size_t, Size),
    FIELD(Address, AreaEnd),
    FIELD(Heap*, Heap),
    FIELD(Address, AreaStart),
    FIELD(Address, Owner),
    // MutablePageMetadata fields:
    FIELD(SlotSet* [kNumSets], SlotSet),
    FIELD(TypedSlotsSet* [kNumSets], TypedSlotSet),
    FIELD(ProgressBar, ProgressBar),
    FIELD(std::atomic<intptr_t>, LiveByteCount),
    FIELD(base::Mutex*, Mutex),
    FIELD(base::SharedMutex*, SharedMutex),
    FIELD(base::Mutex*, PageProtectionChangeMutex),
    FIELD(std::atomic<intptr_t>, ConcurrentSweeping),
    FIELD(std::atomic<size_t>[kNumTypes], ExternalBackingStoreBytes),
    FIELD(heap::ListNode<MutablePageMetadata>, ListNode),
    FIELD(FreeListCategory**, Categories),
    FIELD(PossiblyEmptyBuckets, PossiblyEmptyBuckets),
    FIELD(ActiveSystemPages*, ActiveSystemPages),
    FIELD(size_t, AllocatedLabSize),
    FIELD(size_t, AgeInNewSpace),
    FIELD(MarkingBitmap, MarkingBitmap),
    kEndOfMarkingBitmap,
    kMutablePageMetadataStart = kSlotSetOffset,
    kMemoryChunkMetadataSize = kMutablePageMetadataStart,
    kMutablePageMetadataSize =
        kEndOfMarkingBitmap +
        ((kEndOfMarkingBitmap % kMemoryChunkAlignment) == 0
             ? 0
             : kMemoryChunkAlignment -
                   (kEndOfMarkingBitmap % kMemoryChunkAlignment)),
  };
#undef FIELD

  // Code pages have padding on the first page for code alignment, so the
  // ObjectStartOffset will not be page aligned.
  static constexpr intptr_t ObjectStartOffsetInCodePage() {
    // The instruction stream data (so after the header) should be aligned to
    // kCodeAlignment.
    return RoundUp(sizeof(MemoryChunk) + InstructionStream::kHeaderSize,
                   kCodeAlignment) -
           InstructionStream::kHeaderSize;
  }

  static constexpr size_t AllocatableMemoryInCodePage() {
    return kRegularPageSize - ObjectStartOffsetInCodePage();
  }

  static constexpr size_t ObjectStartOffsetInDataPage() {
    return RoundUp(int{kMemoryChunkHeaderSize},
                   ALIGN_TO_ALLOCATION_ALIGNMENT(kDoubleSize));
  }

  static constexpr size_t AllocatableMemoryInDataPage() {
    constexpr size_t kAllocatableMemoryInDataPage =
        kRegularPageSize - ObjectStartOffsetInDataPage();
    static_assert(kMaxRegularHeapObjectSize <= kAllocatableMemoryInDataPage);
    return kAllocatableMemoryInDataPage;
  }

  static constexpr size_t ObjectStartOffsetInMemoryChunk(
      AllocationSpace space) {
    if (IsAnyCodeSpace(space)) {
      return ObjectStartOffsetInCodePage();
    }
    // Read-only pages use the same layout as regular pages.
    return ObjectStartOffsetInDataPage();
  }

  static constexpr size_t AllocatableMemoryInMemoryChunk(
      AllocationSpace space) {
    DCHECK_NE(space, CODE_LO_SPACE);
    if (space == CODE_SPACE) {
      return AllocatableMemoryInCodePage();
    }
    // Read-only pages use the same layout as regular pages.
    return AllocatableMemoryInDataPage();
  }

  static constexpr int MaxRegularCodeObjectSize() {
    constexpr int kMaxRegularCodeObjectSize = static_cast<int>(
        RoundDown(AllocatableMemoryInCodePage() / 2, kTaggedSize));
    static_assert(kMaxRegularCodeObjectSize <= kMaxRegularHeapObjectSize);
    return kMaxRegularCodeObjectSize;
  }

  static_assert(kMemoryChunkHeaderSize % alignof(size_t) == 0);
};

}  // namespace v8::internal

#endif  // V8_HEAP_MEMORY_CHUNK_LAYOUT_H_
