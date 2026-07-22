// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-chunk-metadata.h"

#include <cstdlib>

#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/marking-inl.h"
#include "src/objects/heap-object.h"
#include "src/utils/allocation.h"

namespace v8::internal {

MemoryChunkMetadata::MemoryChunkMetadata(Heap* heap, BaseSpace* space,
                                         size_t chunk_size, Address area_start,
                                         Address area_end,
                                         VirtualMemory reservation,
                                         Executability executability)
    : reservation_(std::move(reservation)),
      allocated_bytes_(area_end - area_start),
      high_water_mark_(area_start -
                       MemoryChunk::FromAddress(area_start)->address()),
      size_(chunk_size),
      area_end_(area_end),
      heap_(heap),
      area_start_(area_start),
      owner_(space) {
  flags_ = IsExecutableField::update(
      flags_, executability == Executability::EXECUTABLE);
  // Executable chunks are also trusted as they contain machine code and live
  // outside the sandbox (when it is enabled). While mostly symbolic, this is
  // needed for two reasons:
  // 1. We have the invariant that IsTrustedObject(obj) implies
  //    IsTrustedSpaceObject(obj), where IsTrustedSpaceObject checks the
  //   MemoryChunk::IS_TRUSTED flag on the host chunk. As InstructionStream
  //   objects are trusted, their host chunks must also be marked as such.
  // 2. References between trusted objects must use the TRUSTED_TO_TRUSTED
  //    remembered set. However, that will only be used if both the host
  //    and the value chunk are marked as IS_TRUSTED.
  flags_ = IsTrustedField::update(
      flags_, IsAnyTrustedSpace(owner()->identity()) ||
                  executability == Executability::EXECUTABLE);
  // "Trusted" chunks should never be located inside the sandbox as they
  // couldn't be trusted in that case.
  DCHECK_IMPLIES(is_trusted(), OutsideSandbox(ChunkAddress()));
  flags_ = IsWritableSharedSpaceField::update(
      flags_, IsAnyWritableSharedSpace(owner()->identity()));
}

MemoryChunkMetadata::~MemoryChunkMetadata() {
#ifdef V8_ENABLE_SANDBOX
  MemoryChunk::ClearMetadataPointer(this);
#endif  // V8_ENABLE_SANDBOX
}

#ifdef THREAD_SANITIZER
void MemoryChunkMetadata::SynchronizedHeapLoad() const {
  CHECK(reinterpret_cast<Heap*>(
            base::Acquire_Load(reinterpret_cast<base::AtomicWord*>(&(
                const_cast<MemoryChunkMetadata*>(this)->heap_)))) != nullptr ||
        Chunk()->IsFlagSet(MemoryChunk::READ_ONLY_HEAP));
}

void MemoryChunkMetadata::SynchronizedHeapStore() {
  // Since TSAN does not process memory fences, we use the following annotation
  // to tell TSAN that there is no data race when emitting a
  // InitializationMemoryFence. Note that the other thread still needs to
  // perform MutablePageMetadata::synchronized_heap().
  base::Release_Store(reinterpret_cast<base::AtomicWord*>(&heap_),
                      reinterpret_cast<base::AtomicWord>(heap_));
}
#endif

#ifdef DEBUG
bool MemoryChunkMetadata::is_trusted() const {
  const bool is_trusted = IsTrustedField::decode(flags_);
  const bool is_trusted_owner = owner()
                                    ? IsAnyTrustedSpace(owner()->identity()) ||
                                          IsAnyCodeSpace(owner()->identity())
                                    : false;
  DCHECK_EQ(is_trusted, is_trusted_owner);
  return is_trusted;
}
#endif  // DEBUG

}  // namespace v8::internal
