// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_CHUNK_INL_H_
#define V8_HEAP_MEMORY_CHUNK_INL_H_

#include "src/heap/memory-chunk.h"
// Include the non-inl header before the rest of the headers.

#include "src/execution/isolate-inl.h"
#include "src/heap/base-page.h"
#include "src/sandbox/check.h"

namespace v8::internal {

template <bool check_isolate>
BasePage* MemoryChunk::MetadataImpl(const Isolate* isolate) {
  // If this changes, we also need to update
  // `CodeStubAssembler::BasePageFromMemoryChunk()`
#ifdef V8_ENABLE_SANDBOX
  DCHECK_LT(metadata_index_,
            MemoryChunkConstants::kMetadataPointerTableSizeMask);
  IsolateGroup::BasePageTableEntry* metadata_pointer_table =
      IsolateGroup::current()->metadata_pointer_table();
  auto metadata_entry = metadata_pointer_table
      [metadata_index_ & MemoryChunkConstants::kMetadataPointerTableSizeMask];
  if constexpr (check_isolate) {
    metadata_entry.CheckIfMetadataAccessibleFromIsolate(isolate);
  }
  BasePage* metadata = metadata_entry.metadata();
  // Check that the Metadata belongs to this Chunk, since an attacker with write
  // inside the sandbox could've swapped the index. This should be a
  // `SBXCHECK_EQ()` which doesn't currently work as we don't allow nesting of
  // DisallowSandboxAccess in AllowSandboxAccess scopes. There's no sandbox
  // access in the condition so this replacement is fine.
  CHECK_EQ(metadata->Chunk(), this);
  return metadata;
#else
  return metadata_;
#endif
}

template <bool check_isolate>
const BasePage* MemoryChunk::MetadataImpl(const Isolate* isolate) const {
  return const_cast<MemoryChunk*>(this)->MetadataImpl<check_isolate>(isolate);
}

BasePage* MemoryChunk::Metadata(const Isolate* isolate) {
  return MetadataImpl<true>(isolate);
}

const BasePage* MemoryChunk::Metadata(const Isolate* isolate) const {
  return const_cast<MemoryChunk*>(this)->Metadata(isolate);
}

BasePage* MemoryChunk::Metadata() {
  return const_cast<MemoryChunk*>(this)->MetadataImpl<true>(Isolate::Current());
}

const BasePage* MemoryChunk::Metadata() const {
  return const_cast<MemoryChunk*>(this)->Metadata();
}

BasePage* MemoryChunk::MetadataNoIsolateCheck() {
  return MetadataImpl<false>(nullptr);
}

const BasePage* MemoryChunk::MetadataNoIsolateCheck() const {
  return const_cast<MemoryChunk*>(this)->MetadataImpl<false>(nullptr);
}

bool MemoryChunk::IsEvacuationCandidate() const {
#ifdef DEBUG
  const auto* metadata = Metadata();
  DCHECK((!metadata->never_evacuate() || !IsFlagSet(EVACUATION_CANDIDATE)));
  DCHECK_EQ(metadata->is_evacuation_candidate(),
            IsFlagSet(EVACUATION_CANDIDATE));
#endif  // DEBUG
  return IsFlagSet(EVACUATION_CANDIDATE);
}

V8_INLINE bool MemoryChunk::InReadOnlySpace() const {
  // This is needed because TSAN does not process the memory fence
  // emitted after page initialization.
  SynchronizedLoad();
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  const bool ro_via_address =
      (reinterpret_cast<uintptr_t>(this) & kContiguousReadOnlySpaceMask) == 0;
  DCHECK_IMPLIES(ro_via_address, Metadata()->IsReadOnlyPage());
  return ro_via_address;
#else  // !CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  return IsFlagSet(READ_ONLY_HEAP);
#endif
}

}  // namespace v8::internal

#endif  // V8_HEAP_MEMORY_CHUNK_INL_H_
