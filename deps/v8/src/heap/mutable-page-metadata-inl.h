// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MUTABLE_PAGE_METADATA_INL_H_
#define V8_HEAP_MUTABLE_PAGE_METADATA_INL_H_

#include "src/heap/mutable-page-metadata.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/memory-chunk-metadata-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/sandbox/hardware-support.h"

namespace v8 {
namespace internal {

// static
MutablePageMetadata* MutablePageMetadata::FromAddress(const Isolate* i,
                                                      Address a) {
  return cast(MemoryChunkMetadata::FromAddress(i, a));
}

// static
MutablePageMetadata* MutablePageMetadata::FromHeapObject(const Isolate* i,
                                                         Tagged<HeapObject> o) {
  return cast(MemoryChunkMetadata::FromHeapObject(i, o));
}

template <AccessMode mode>
void MutablePageMetadata::ClearLiveness() {
  marking_bitmap()->Clear<mode>();
  SetLiveBytes(0);
}

void MutablePageMetadata::SetMajorGCInProgress() {
#if V8_ENABLE_STICKY_MARK_BITS_BOOL
  DCHECK(v8_flags.sticky_mark_bits);
  SetFlagUnlocked(MemoryChunk::STICKY_MARK_BIT_IS_MAJOR_GC_IN_PROGRESS);
#else
  UNREACHABLE();
#endif
}

void MutablePageMetadata::ResetMajorGCInProgress() {
#if V8_ENABLE_STICKY_MARK_BITS_BOOL
  DCHECK(v8_flags.sticky_mark_bits);
  ClearFlagUnlocked(MemoryChunk::STICKY_MARK_BIT_IS_MAJOR_GC_IN_PROGRESS);
#else
  UNREACHABLE();
#endif
}

void MutablePageMetadata::ClearFlagsNonExecutable(
    MemoryChunk::MainThreadFlags flags) {
  return ClearFlagsUnlocked(flags);
}

void MutablePageMetadata::SetFlagsNonExecutable(
    MemoryChunk::MainThreadFlags flags, MemoryChunk::MainThreadFlags mask) {
  return SetFlagsUnlocked(flags, mask);
}

void MutablePageMetadata::ClearFlagNonExecutable(MemoryChunk::Flag flag) {
  return ClearFlagUnlocked(flag);
}

void MutablePageMetadata::SetFlagNonExecutable(MemoryChunk::Flag flag) {
  return SetFlagUnlocked(flag);
}

V8_INLINE void MutablePageMetadata::RawSetTrustedAndUntrustedFlags(
    MemoryChunk::MainThreadFlags new_flags) {
#ifdef V8_ENABLE_SANDBOX
  // Must copy out as the SBXCHECK() macros are not allowed to access the
  // sandbox as that's racy. Copying out is okay as we merely want to shrink the
  // time window here.
  const auto untrusted_main_thread_flags =
      Chunk()->untrusted_main_thread_flags_;
  SBXCHECK_EQ(untrusted_main_thread_flags, trusted_main_thread_flags_);
#endif  // V8_ENABLE_SANDBOX
  trusted_main_thread_flags_ = new_flags;
  Chunk()->untrusted_main_thread_flags_ = trusted_main_thread_flags_;
}

void MutablePageMetadata::SetFlagsUnlocked(MemoryChunk::MainThreadFlags flags,
                                           MemoryChunk::MainThreadFlags mask) {
  RawSetTrustedAndUntrustedFlags((trusted_main_thread_flags_ & ~mask) |
                                 (flags & mask));
}

void MutablePageMetadata::ClearFlagsUnlocked(
    MemoryChunk::MainThreadFlags flags) {
  RawSetTrustedAndUntrustedFlags(trusted_main_thread_flags_ & ~flags);
}

void MutablePageMetadata::SetFlagUnlocked(MemoryChunk::Flag flag) {
  RawSetTrustedAndUntrustedFlags(trusted_main_thread_flags_ | flag);
}

void MutablePageMetadata::ClearFlagUnlocked(MemoryChunk::Flag flag) {
  RawSetTrustedAndUntrustedFlags(trusted_main_thread_flags_.without(flag));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MUTABLE_PAGE_METADATA_INL_H_
