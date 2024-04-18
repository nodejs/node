// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-chunk-header.h"

#include "src/heap/base-space.h"
#include "src/heap/basic-memory-chunk.h"

namespace v8 {
namespace internal {

#ifdef THREAD_SANITIZER

bool MemoryChunkHeader::InReadOnlySpace() const {
  // This is needed because TSAN does not process the memory fence
  // emitted after page initialization.
  MemoryChunk()->SynchronizedHeapLoad();
  return IsFlagSet(READ_ONLY_HEAP);
}

#endif  // THREAD_SANITIZER

#ifdef DEBUG

bool MemoryChunkHeader::IsTrusted() const {
  bool is_trusted = IsFlagSet(IS_TRUSTED);
  DCHECK_EQ(is_trusted,
            MemoryChunk()->owner()->identity() == TRUSTED_SPACE ||
                MemoryChunk()->owner()->identity() == TRUSTED_LO_SPACE);
  return is_trusted;
}

#endif  // DEBUG

Heap* MemoryChunkHeader::GetHeap() { return MemoryChunk()->heap(); }

}  // namespace internal
}  // namespace v8
