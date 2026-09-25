// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-chunk.h"

#include "src/common/code-memory-access-inl.h"
#include "src/heap/base-space.h"
#include "src/heap/large-page.h"
#include "src/heap/normal-page.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/trusted-range.h"

namespace v8 {
namespace internal {

// This check is here to ensure that the lower 32 bits of any real heap object
// can't overlap with the lower 32 bits of cleared weak reference value and
// therefore it's enough to compare only the lower 32 bits of a
// Tagged<MaybeObject> in order to figure out if it's a cleared weak reference
// or not.
static_assert(kClearedWeakHeapObjectLower32 > 0);
static_assert(kClearedWeakHeapObjectLower32 < sizeof(MemoryChunk));

// static
constexpr MemoryChunk::MainThreadFlags MemoryChunk::kAllFlagsMask;
// static
constexpr MemoryChunk::MainThreadFlags
    MemoryChunk::kPointersToHereAreInterestingMask;
// static
constexpr MemoryChunk::MainThreadFlags
    MemoryChunk::kPointersFromHereAreInterestingMask;
// static
constexpr MemoryChunk::MainThreadFlags MemoryChunk::kEvacuationCandidateMask;
// static
constexpr MemoryChunk::MainThreadFlags MemoryChunk::kIsInYoungGenerationMask;
// static
constexpr MemoryChunk::MainThreadFlags MemoryChunk::kIsLargePageMask;
// static
constexpr MemoryChunk::MainThreadFlags
    MemoryChunk::kSkipEvacuationSlotsRecordingMask;

MemoryChunk::MemoryChunk(MainThreadFlags flags, BasePage* metadata)
    : untrusted_main_thread_flags_(flags)
#ifndef V8_ENABLE_SANDBOX
      ,
      metadata_(metadata)
#endif
{
#ifdef V8_ENABLE_SANDBOX
  auto metadata_index = MetadataTableIndex(address());
  IsolateGroup::BasePageTableEntry* metadata_pointer_table =
      MetadataTableAddress();
  DCHECK_IMPLIES(metadata_pointer_table[metadata_index].metadata() != nullptr,
                 metadata_pointer_table[metadata_index].metadata() == metadata);
  metadata_pointer_table[metadata_index].SetMetadata(
      metadata, metadata->heap()->isolate());
  metadata_index_ = metadata_index;
#endif
}

#ifdef V8_ENABLE_SANDBOX
// static
void MemoryChunk::ClearMetadataPointer(BasePage* metadata) {
  uint32_t metadata_index = MetadataTableIndex(metadata->ChunkAddress());
  IsolateGroup::BasePageTableEntry* metadata_pointer_table =
      MetadataTableAddress();
  IsolateGroup::BasePageTableEntry& chunk_metadata =
      metadata_pointer_table[metadata_index];
  if (chunk_metadata.metadata() == nullptr) {
    DCHECK_EQ(chunk_metadata.isolate(), nullptr);
    return;
  }
  CHECK_EQ(chunk_metadata.metadata(), metadata);
  metadata_pointer_table[metadata_index].SetMetadata(nullptr, nullptr);
}

// static
uint32_t MemoryChunk::MetadataTableIndex(Address chunk_address) {
  uint32_t index;
  if (V8HeapCompressionScheme::GetPtrComprCageBaseAddress(chunk_address) ==
      V8HeapCompressionScheme::base()) {
    static_assert(kPtrComprCageReservationSize == kPtrComprCageBaseAlignment);
    Tagged_t offset = V8HeapCompressionScheme::CompressAny(chunk_address);
    DCHECK_LT(offset >> kPageSizeBits, MemoryChunkConstants::kPagesInMainCage);
    index = MemoryChunkConstants::kMainCageMetadataOffset +
            (offset >> kPageSizeBits);
  } else if (IsolateGroup::current()
                 ->GetTrustedPtrComprCage()
                 ->region()
                 .contains(chunk_address)) {
    Tagged_t offset = TrustedSpaceCompressionScheme::CompressAny(chunk_address);
    DCHECK_LT(offset >> kPageSizeBits,
              MemoryChunkConstants::kPagesInTrustedCage);
    index = MemoryChunkConstants::kTrustedSpaceMetadataOffset +
            (offset >> kPageSizeBits);
  } else {
    CodeRange* code_range = IsolateGroup::current()->GetCodeRange();
    DCHECK(code_range->region().contains(chunk_address));
    uint32_t offset = static_cast<uint32_t>(chunk_address - code_range->base());
    DCHECK_LT(offset >> kPageSizeBits, MemoryChunkConstants::kPagesInCodeCage);
    index = MemoryChunkConstants::kCodeRangeMetadataOffset +
            (offset >> kPageSizeBits);
  }
  DCHECK_LT(index, MemoryChunkConstants::kMetadataPointerTableSize);
  return index;
}

bool MemoryChunk::SandboxSafeInReadOnlySpace() const {
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  // With contiguous read-only space the fact that memory is read-only is based
  // on its address and there's no way to corrupt that.
  return InReadOnlySpace();
#else   // !CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  // For the sandbox only flags from writable pages can be corrupted so we can
  // use the flag check as a fast path in this case.
  // It also helps making TSAN happy, since it doesn't like the way we
  // initialize the MemoryChunks.
  // (See BasePage::SynchronizedHeapLoad).
  if (!InReadOnlySpace()) {
    return false;
  }
  SBXCHECK_EQ(static_cast<const ReadOnlyPage*>(Metadata())->ChunkAddress(),
              address());
  return true;
#endif  // !CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
}

#endif  // V8_ENABLE_SANDBOX

void MemoryChunk::InitializationMemoryFence() {
  base::SeqCst_MemoryFence();

#ifdef THREAD_SANITIZER
  // Since TSAN does not process memory fences, we use the following annotation
  // to tell TSAN that there is no data race when emitting a
  // InitializationMemoryFence. Note that the other thread still needs to
  // perform MutablePage::synchronized_heap().
  Metadata()->SynchronizedHeapStore();
#ifndef V8_ENABLE_SANDBOX
  base::Release_Store(reinterpret_cast<base::AtomicWord*>(&metadata_),
                      reinterpret_cast<base::AtomicWord>(metadata_));
#else
  IsolateGroup::BasePageTableEntry* metadata_pointer_table =
      MetadataTableAddress();
  static_assert(sizeof(base::AtomicWord) ==
                sizeof(metadata_pointer_table[0].metadata()));
  static_assert(sizeof(base::Atomic32) == sizeof(metadata_index_));
  base::Release_Store(
      reinterpret_cast<base::AtomicWord*>(
          metadata_pointer_table[metadata_index_].metadata_slot()),
      reinterpret_cast<base::AtomicWord>(
          metadata_pointer_table[metadata_index_].metadata()));
  base::Release_Store(reinterpret_cast<base::Atomic32*>(&metadata_index_),
                      metadata_index_);
#endif
#endif
}

#ifdef THREAD_SANITIZER

void MemoryChunk::SynchronizedLoad() const {
#ifndef V8_ENABLE_SANDBOX
  BasePage* metadata = reinterpret_cast<BasePage*>(
      base::Acquire_Load(reinterpret_cast<base::AtomicWord*>(
          &(const_cast<MemoryChunk*>(this)->metadata_))));
#else
  IsolateGroup::BasePageTableEntry* metadata_pointer_table =
      MetadataTableAddress();
  static_assert(sizeof(base::AtomicWord) ==
                sizeof(metadata_pointer_table[0].metadata()));
  static_assert(sizeof(base::Atomic32) == sizeof(metadata_index_));
  uint32_t metadata_index =
      base::Acquire_Load(reinterpret_cast<base::Atomic32*>(
          &(const_cast<MemoryChunk*>(this)->metadata_index_)));
  BasePage* metadata = reinterpret_cast<BasePage*>(
      base::Acquire_Load(reinterpret_cast<base::AtomicWord*>(
          metadata_pointer_table[metadata_index].metadata_slot())));
#endif
  metadata->SynchronizedHeapLoad();
}

#endif  // THREAD_SANITIZER

#ifdef DEBUG

size_t MemoryChunk::Offset(Address addr) const {
  DCHECK_GE(addr, Metadata()->area_start());
  DCHECK_LE(addr, address() + Metadata()->size());
  return addr - address();
}

size_t MemoryChunk::OffsetMaybeOutOfRange(Address addr) const {
  DCHECK_GE(addr, Metadata()->area_start());
  return addr - address();
}

#endif  // DEBUG

}  // namespace internal
}  // namespace v8
