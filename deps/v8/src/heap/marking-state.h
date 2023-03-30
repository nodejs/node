// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_STATE_H_
#define V8_HEAP_MARKING_STATE_H_

#include "src/common/globals.h"
#include "src/heap/marking.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

class BasicMemoryChunk;
class MemoryChunk;

template <typename ConcreteState, AccessMode access_mode>
class MarkingStateBase {
 public:
  // Declares that this marking state is not collecting retainers, so the
  // marking visitor may update the heap state to store information about
  // progress, and may avoid fully visiting an object if it is safe to do so.
  static constexpr bool kCollectRetainers = false;

  explicit MarkingStateBase(PtrComprCageBase cage_base)
#if V8_COMPRESS_POINTERS
      : cage_base_(cage_base)
#endif
  {
  }

  // The pointer compression cage base value used for decompression of all
  // tagged values except references to InstructionStream objects.
  V8_INLINE PtrComprCageBase cage_base() const {
#if V8_COMPRESS_POINTERS
    return cage_base_;
#else
    return PtrComprCageBase{};
#endif  // V8_COMPRESS_POINTERS
  }

  V8_INLINE MarkBit MarkBitFrom(const HeapObject obj) const;

  // {addr} may be tagged or aligned.
  V8_INLINE MarkBit MarkBitFrom(const BasicMemoryChunk* p, Address addr) const;

  V8_INLINE bool IsImpossible(const HeapObject obj) const;
  V8_INLINE bool IsGrey(const HeapObject obj) const;
  V8_INLINE bool IsBlackOrGrey(const HeapObject obj) const;
  V8_INLINE bool GreyToBlack(HeapObject obj);

  V8_INLINE bool TryMark(HeapObject obj);
  // Helper method for fully marking an object and accounting its live bytes.
  // Should be used to mark individual objects in one-off cases.
  V8_INLINE bool TryMarkAndAccountLiveBytes(HeapObject obj);
  V8_INLINE bool IsMarked(const HeapObject obj) const;
  V8_INLINE bool IsUnmarked(const HeapObject obj) const;

  V8_INLINE void ClearLiveness(MemoryChunk* chunk);

  void AddStrongReferenceForReferenceSummarizer(HeapObject host,
                                                HeapObject obj) {
    // This is not a reference summarizer, so there is nothing to do here.
  }

  void AddWeakReferenceForReferenceSummarizer(HeapObject host, HeapObject obj) {
    // This is not a reference summarizer, so there is nothing to do here.
  }

 private:
#if V8_COMPRESS_POINTERS
  const PtrComprCageBase cage_base_;
#endif  // V8_COMPRESS_POINTERS
};

// This is used by marking visitors.
class MarkingState final
    : public MarkingStateBase<MarkingState, AccessMode::ATOMIC> {
 public:
  explicit MarkingState(PtrComprCageBase cage_base)
      : MarkingStateBase(cage_base) {}

  V8_INLINE ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const;

  // Concurrent marking uses local live bytes so we may do these accesses
  // non-atomically.
  V8_INLINE void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by);

  V8_INLINE intptr_t live_bytes(const MemoryChunk* chunk) const;

  V8_INLINE void SetLiveBytes(MemoryChunk* chunk, intptr_t value);
};

class NonAtomicMarkingState final
    : public MarkingStateBase<NonAtomicMarkingState, AccessMode::NON_ATOMIC> {
 public:
  explicit NonAtomicMarkingState(PtrComprCageBase cage_base)
      : MarkingStateBase(cage_base) {}

  V8_INLINE ConcurrentBitmap<AccessMode::NON_ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const;

  V8_INLINE void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by);

  V8_INLINE intptr_t live_bytes(const MemoryChunk* chunk) const;

  V8_INLINE void SetLiveBytes(MemoryChunk* chunk, intptr_t value);
};

// This is used by Scavenger and Evacuator in TransferColor.
// Live byte increments have to be atomic.
class AtomicMarkingState final
    : public MarkingStateBase<AtomicMarkingState, AccessMode::ATOMIC> {
 public:
  explicit AtomicMarkingState(PtrComprCageBase cage_base)
      : MarkingStateBase(cage_base) {}

  V8_INLINE ConcurrentBitmap<AccessMode::ATOMIC>* bitmap(
      const BasicMemoryChunk* chunk) const;

  V8_INLINE void IncrementLiveBytes(MemoryChunk* chunk, intptr_t by);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_STATE_H_
