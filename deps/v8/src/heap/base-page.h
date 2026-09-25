// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_PAGE_H_
#define V8_HEAP_BASE_PAGE_H_

#include "src/base/bit-field.h"
#include "src/base/hashing.h"
#include "src/common/globals.h"
#include "src/heap/base-space.h"
#include "src/heap/memory-chunk.h"
#include "src/objects/heap-object.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

namespace debug_helper_internal {
class ReadStringVisitor;
}  // namespace  debug_helper_internal

class BaseSpace;

// Base class for all pages of any size or type on the heap.
//
// For the purpose of the V8 sandbox the a page always resides in trusted
// memory.
class BasePage {
 public:
  // Only correct if the pointer is in the first kPageSize of the MemoryChunk.
  // This is not necessarily the case for large objects.
  V8_INLINE static BasePage* FromAddress(Address a);
  V8_INLINE static BasePage* FromAddress(const Isolate* isolate, Address a);

  // Objects pointers always point within the first kPageSize, so these calls
  // are always correct.
  V8_INLINE static BasePage* FromHeapObject(Tagged<HeapObject> o);
  V8_INLINE static BasePage* FromHeapObject(const Isolate* i,
                                            Tagged<HeapObject> o);
  V8_INLINE static BasePage* FromHeapObject(const Isolate* i,
                                            const HeapObjectLayout* o);

  // Updates the high-water mark of the page that corresponds to `mark`. Can be
  // passed a limit which points one byte past the current page.
  V8_INLINE static void UpdateHighWaterMark(Address mark);

  ~BasePage();

  bool IsReadOnlyPage() const { return IsReadOnlyPageField::decode(flags_); }
  bool IsMutablePage() const { return !IsReadOnlyPage(); }
  bool IsLargePage() const {
    const bool large = is_large();
    DCHECK_IMPLIES(large, IsMutablePage());
    return large;
  }
  bool IsNormalPage() const { return IsMutablePage() && !IsLargePage(); }

  Address ChunkAddress() const { return Chunk()->address(); }
  Address MetadataAddress() const { return reinterpret_cast<Address>(this); }

  // Returns the offset of a given address to this page.
  inline size_t Offset(Address a) const { return Chunk()->Offset(a); }

  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }

  Address area_start() const { return area_start_; }

  Address area_end() const { return area_end_; }
  void set_area_end(Address area_end) { area_end_ = area_end; }

  size_t area_size() const {
    return static_cast<size_t>(area_end() - area_start());
  }

  Heap* heap() const {
    DCHECK_NOT_NULL(heap_);
    return heap_;
  }

  // Gets the page's owner or null if the page has been detached from relevant
  // spaces.
  BaseSpace* owner() const { return owner_; }
  void set_owner(BaseSpace* space) { owner_ = space; }
  // Gets the chunk's allocation space, potentially dealing with a null
  // `owner()` (like read-only chunks have).
  inline AllocationSpace owner_identity() const;

  inline bool IsWritable() const;

  bool Contains(Address addr) const {
    return addr >= area_start() && addr < area_end();
  }

  // Checks whether |addr| can be a limit of addresses in this page. It's a
  // limit if it's in the page, or if it's just after the last byte of the page.
  bool ContainsLimit(Address addr) const {
    return addr >= area_start() && addr <= area_end();
  }

  size_t wasted_memory() const { return wasted_memory_; }
  void add_wasted_memory(size_t waste) { wasted_memory_ += waste; }
  size_t allocated_bytes() const { return allocated_bytes_; }

  Address HighWaterMark() const { return ChunkAddress() + high_water_mark_; }

  VirtualMemory* reserved_memory() { return &reservation_; }

  void ResetAllocationStatistics() {
    allocated_bytes_ = area_size();
    wasted_memory_ = 0;
  }

  void IncreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    allocated_bytes_ += bytes;
  }

  void DecreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    DCHECK_GE(allocated_bytes(), bytes);
    allocated_bytes_ -= bytes;
  }

  MemoryChunk* Chunk() { return MemoryChunk::FromAddress(area_start()); }
  const MemoryChunk* Chunk() const {
    return MemoryChunk::FromAddress(area_start());
  }

  bool is_pinned_for_testing() const {
    const bool is_pinned = IsPinnedForTestingField::decode(flags_);
    DCHECK_IMPLIES(is_pinned, !is_forced_evacuation_candidate_for_testing());
    return is_pinned;
  }
  void set_is_pinned_for_testing(bool value) {
    flags_ = IsPinnedForTestingField::update(flags_, value);
  }

  bool is_unregistered() const { return IsUnregisteredField::decode(flags_); }
  void set_is_unregistered() {
    // Metadata will be re-initialized before being reused.
    DCHECK(!is_unregistered());
    flags_ = IsUnregisteredField::update(flags_, true);
  }

  bool is_pre_freed() const { return IsPreeFreedField::decode(flags_); }
  void set_is_pre_freed() {
    // Metadata will be re-initialized before being reused.
    DCHECK(!is_pre_freed());
    flags_ = IsPreeFreedField::update(flags_, true);
  }

  bool is_large() const { return IsLargePageField::decode(flags_); }
  void set_is_large() {
    // Metadata will be re-initialized before being reused.
    DCHECK(!is_large());
    flags_ = IsLargePageField::update(flags_, true);
  }

  bool is_executable() const { return IsExecutableField::decode(flags_); }

  bool will_be_promoted() const { return WillBePromotedField::decode(flags_); }
  void set_will_be_promoted(bool value) {
    // Only support toggling the value as we should always know which state we
    // are in.
    DCHECK_EQ(value, !will_be_promoted());
    flags_ = WillBePromotedField::update(flags_, value);
  }

  bool is_quarantined() const { return IsQuarantinedField::decode(flags_); }
  void set_is_quarantined(bool value) {
    // Only support toggling the value as we should always know which state we
    // are in.
    DCHECK_EQ(value, !is_quarantined());
    flags_ = IsQuarantinedField::update(flags_, value);
  }

  bool is_evacuation_candidate() const {
    return IsEvacuationCandidateField::decode(flags_);
  }

  bool evacuation_was_aborted() const {
    const bool value = EvacuationWasAbortedField::decode(flags_);
    // Aborted evacuation candidates are still evacuation candidates.
    DCHECK_IMPLIES(value, is_evacuation_candidate());
    return value;
  }

  bool never_evacuate() const { return NeverEvacuateField::decode(flags_); }

  bool never_allocate_on_chunk() const {
    return NeverAllocateOnChunk::decode(flags_);
  }
  void set_never_allocate_on_chunk(bool value) {
    flags_ = NeverAllocateOnChunk::update(flags_, value);
  }

  bool CanAllocateOnChunk() const {
    return !is_evacuation_candidate() && !never_allocate_on_chunk();
  }

  bool is_forced_evacuation_candidate_for_testing() const {
    return ForceEvacuationCandidateForTestingField::decode(flags_);
  }
  void set_forced_evacuation_candidate_for_testing(bool value) {
    flags_ = ForceEvacuationCandidateForTestingField::update(flags_, value);
  }

#ifdef DEBUG
  V8_EXPORT_PRIVATE bool is_trusted() const;
#else
  bool is_trusted() const { return IsTrustedField::decode(flags_); }
#endif

  bool is_writable_shared() const {
    return IsWritableSharedSpaceField::decode(flags_);
  }

  bool is_black_allocated() const {
    const bool black_allocated = IsBlackAllocated::decode(flags_);
    DCHECK_IMPLIES(black_allocated, v8_flags.black_allocated_pages);
    return black_allocated;
  }

 protected:
#ifdef THREAD_SANITIZER
  // Perform a dummy acquire load to tell TSAN that there is no data race in
  // mark-bit initialization. See MutablePage::Initialize for the
  // corresponding release store.
  void SynchronizedHeapLoad() const;
  void SynchronizedHeapStore();
  friend class MemoryChunk;
#endif

  BasePage(Heap* heap, BaseSpace* space, size_t chunk_size, Address area_start,
           Address area_end, VirtualMemory reservation,
           Executability executability);

  void set_evacuation_was_aborted(bool value) {
    // Only support toggling the value as we should always know which state we
    // are in.
    DCHECK_EQ(value, !evacuation_was_aborted());
    flags_ = EvacuationWasAbortedField::update(flags_, value);
  }

  void set_is_evacuation_candidate(bool value) {
    // Only support toggling the value as we should always know which state we
    // are in.
    DCHECK_EQ(value, !is_evacuation_candidate());
    flags_ = IsEvacuationCandidateField::update(flags_, value);
  }

  void set_never_evacuate() {
    flags_ = NeverEvacuateField::update(flags_, true);
  }

  void set_is_sealed_ro_space() {
    DCHECK(!IsSealedReadOnlySpaceField::decode(flags_));
    flags_ = IsSealedReadOnlySpaceField::update(flags_, true);
  }

  void set_is_read_only_page() {
    DCHECK(!IsReadOnlyPageField::decode(flags_));
    flags_ = IsReadOnlyPageField::update(flags_, true);
  }

  void set_is_black_allocated(bool value) {
    // Only support toggling the value as we should always know which state we
    // are in.
    DCHECK_EQ(value, !is_black_allocated());
    flags_ = IsBlackAllocated::update(flags_, value);
  }

  // If the chunk needs to remember its memory reservation, it is stored here.
  VirtualMemory reservation_;

  // Byte allocated on the page, which includes all objects on the page and the
  // linear allocation area.
  size_t allocated_bytes_;
  // Freed memory that was not added to the free list.
  size_t wasted_memory_ = 0;

  // Assuming the initial allocation on a page is sequential, count highest
  // number of bytes ever allocated on the page.
  std::atomic<intptr_t> high_water_mark_;

  // Overall size of the chunk, including the header and guards.
  size_t size_;

  Address area_end_;

  // The most accessed fields start at heap_ and end at
  // MutablePage::slot_set_. See MutablePage::MutablePage() for details.

  // The heap this chunk belongs to. May be null for read-only chunks.
  Heap* heap_;

  // Start and end of allocatable memory on this chunk.
  Address area_start_;

  // The space owning this memory chunk.
  std::atomic<BaseSpace*> owner_;

  using FlagsT = uint32_t;
  FlagsT flags_ = 0;

 private:
  // The memory chunk is pinned in memory and can't be moved. Only used for
  // testing at this point.
  using IsPinnedForTestingField = v8::base::BitField<bool, 0, 1, FlagsT>;
  // The memory chunk freeing bookkeeping has been performed but the chunk has
  // not yet been freed.
  using IsUnregisteredField = IsPinnedForTestingField::Next<bool, 1>;
  // The memory chunk is already logically freed, however the actual freeing
  // still has to be performed.
  using IsPreeFreedField = IsUnregisteredField::Next<bool, 1>;
  // The memory chunk is large.
  using IsLargePageField = IsPreeFreedField::Next<bool, 1>;
  // Indicates whether the memory chunk is executable or not.
  using IsExecutableField = IsLargePageField::Next<bool, 1>;
  // The memory chunk is flagged to be promoted from the young to the old
  // generation during the final pause of a GC cycle. The flag is used for young
  // and old generation GCs.
  using WillBePromotedField = IsExecutableField::Next<bool, 1>;
  // A quarantined memory chunk contains objects reachable from stack during a
  // GC. Quarantined chunks are not used for further allocations in new
  // space (to make it easier to keep track of the intermediate generation).
  using IsQuarantinedField = WillBePromotedField::Next<bool, 1>;
  // The memory chunk is an evacuation candidate which means that it has been
  // selected for compaction. Slots to such chunks are recorded by the write
  // barrier.
  using IsEvacuationCandidateField = IsQuarantinedField::Next<bool, 1>;
  // Evacuation was aborted on the chunk. These chunks are still evacuation
  // candidates.
  using EvacuationWasAbortedField = IsEvacuationCandidateField::Next<bool, 1>;
  // Indicates whether the memory chunk should never be evacuated.
  using NeverEvacuateField = EvacuationWasAbortedField::Next<bool, 1>;
  // Indicates whether the memory chunk can be used for allocation.
  using NeverAllocateOnChunk = NeverEvacuateField::Next<bool, 1>;
  // This flag is intended to be used for testing. Works only when
  // `v8_flags.manual_evacuation_candidates_selection` is set. It forces the
  // page to become an evacuation candidate at next candidates selection
  // cycle.
  using ForceEvacuationCandidateForTestingField =
      NeverAllocateOnChunk::Next<bool, 1>;
  // The memory chunk belongs to the trusted space. When the sandbox is
  // enabled, the trusted space is located outside of the sandbox and so its
  // content cannot be corrupted by an attacker.
  using IsTrustedField = ForceEvacuationCandidateForTestingField::Next<bool, 1>;
  // The memory chunk belongs to the shared space.
  using IsWritableSharedSpaceField = IsTrustedField::Next<bool, 1>;
  // The memory chunk belongs to a sealed read-only space.
  using IsSealedReadOnlySpaceField = IsWritableSharedSpaceField::Next<bool, 1>;
  // The memory chunk belongs to a read-only space.
  using IsReadOnlyPageField = IsSealedReadOnlySpaceField::Next<bool, 1>;
  // The memory chunk has been black allocated
  using IsBlackAllocated = IsReadOnlyPageField::Next<bool, 1>;

  static constexpr intptr_t HeapOffset() { return offsetof(BasePage, heap_); }

  static constexpr intptr_t AreaStartOffset() {
    return offsetof(BasePage, area_start_);
  }

  static constexpr intptr_t FlagsOffset() { return offsetof(BasePage, flags_); }

  // For HeapOffset().
  friend class debug_helper_internal::ReadStringVisitor;
  // For AreaStartOffset(), FlagsOffset().
  friend class CodeStubAssembler;
  friend class MacroAssembler;
};

}  // namespace internal

namespace base {

// Define special hash function for chunk pointers, to be used with std data
// structures, e.g.
// std::unordered_set<BasePage*, base::hash<BasePage*>
// This hash function discards the trailing zero bits (chunk alignment).
// Notice that, when pointer compression is enabled, it also discards the
// cage base.
template <>
struct hash<const i::BasePage*> {
  V8_INLINE size_t operator()(const i::BasePage* chunk_metadata) const {
    return hash<const i::MemoryChunk*>()(chunk_metadata->Chunk());
  }
};

template <>
struct hash<i::BasePage*> {
  V8_INLINE size_t operator()(i::BasePage* chunk_metadata) const {
    return hash<const i::BasePage*>()(chunk_metadata);
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_HEAP_BASE_PAGE_H_
