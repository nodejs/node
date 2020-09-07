// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_READ_ONLY_SPACES_H_
#define V8_HEAP_READ_ONLY_SPACES_H_

#include <memory>
#include <utility>

#include "include/v8-platform.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/heap/allocation-stats.h"
#include "src/heap/base-space.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/list.h"
#include "src/heap/memory-chunk.h"

namespace v8 {
namespace internal {

class MemoryAllocator;
class ReadOnlyHeap;

class ReadOnlyPage : public BasicMemoryChunk {
 public:
  // Clears any pointers in the header that point out of the page that would
  // otherwise make the header non-relocatable.
  void MakeHeaderRelocatable();

  size_t ShrinkToHighWaterMark();

 private:
  friend class ReadOnlySpace;
};

// -----------------------------------------------------------------------------
// Artifacts used to construct a new SharedReadOnlySpace
class ReadOnlyArtifacts {
 public:
  ~ReadOnlyArtifacts();

  void set_accounting_stats(const AllocationStats& stats) { stats_ = stats; }

  void set_shared_read_only_space(
      std::unique_ptr<SharedReadOnlySpace> shared_space) {
    shared_read_only_space_ = std::move(shared_space);
  }
  SharedReadOnlySpace* shared_read_only_space() {
    return shared_read_only_space_.get();
  }

  std::vector<ReadOnlyPage*>& pages() { return pages_; }
  void TransferPages(std::vector<ReadOnlyPage*>&& pages) {
    pages_ = std::move(pages);
  }

  const AllocationStats& accounting_stats() const { return stats_; }

  void set_read_only_heap(std::unique_ptr<ReadOnlyHeap> read_only_heap);
  ReadOnlyHeap* read_only_heap() { return read_only_heap_.get(); }

 private:
  std::vector<ReadOnlyPage*> pages_;
  AllocationStats stats_;
  std::unique_ptr<SharedReadOnlySpace> shared_read_only_space_;
  std::unique_ptr<ReadOnlyHeap> read_only_heap_;
};

// -----------------------------------------------------------------------------
// Read Only space for all Immortal Immovable and Immutable objects
class ReadOnlySpace : public BaseSpace {
 public:
  V8_EXPORT_PRIVATE explicit ReadOnlySpace(Heap* heap);

  // Detach the pages and them to artifacts for using in creating a
  // SharedReadOnlySpace.
  void DetachPagesAndAddToArtifacts(
      std::shared_ptr<ReadOnlyArtifacts> artifacts);

  V8_EXPORT_PRIVATE ~ReadOnlySpace() override;

  bool IsDetached() const { return heap_ == nullptr; }

  bool writable() const { return !is_marked_read_only_; }

  bool Contains(Address a) = delete;
  bool Contains(Object o) = delete;

  V8_EXPORT_PRIVATE
  AllocationResult AllocateRaw(size_t size_in_bytes,
                               AllocationAlignment alignment);

  V8_EXPORT_PRIVATE void ClearStringPaddingIfNeeded();

  enum class SealMode { kDetachFromHeapAndForget, kDoNotDetachFromHeap };

  // Seal the space by marking it read-only, optionally detaching it
  // from the heap and forgetting it for memory bookkeeping purposes (e.g.
  // prevent space's memory from registering as leaked).
  V8_EXPORT_PRIVATE void Seal(SealMode ro_mode);

  // During boot the free_space_map is created, and afterwards we may need
  // to write it into the free space nodes that were already created.
  void RepairFreeSpacesAfterDeserialization();

  size_t Size() override { return accounting_stats_.Size(); }
  V8_EXPORT_PRIVATE size_t CommittedPhysicalMemory() override;

  const std::vector<ReadOnlyPage*>& pages() const { return pages_; }
  Address top() const { return top_; }
  Address limit() const { return limit_; }
  size_t Capacity() const { return capacity_; }

  bool ContainsSlow(Address addr);
  V8_EXPORT_PRIVATE void ShrinkPages();
#ifdef VERIFY_HEAP
  void Verify(Isolate* isolate);
#ifdef DEBUG
  void VerifyCounters(Heap* heap);
#endif  // DEBUG
#endif  // VERIFY_HEAP

  // Return size of allocatable area on a page in this space.
  int AreaSize() { return static_cast<int>(area_size_); }

  ReadOnlyPage* InitializePage(BasicMemoryChunk* chunk);

  Address FirstPageAddress() const { return pages_.front()->address(); }

 protected:
  void SetPermissionsForPages(MemoryAllocator* memory_allocator,
                              PageAllocator::Permission access);

  bool is_marked_read_only_ = false;

  // Accounting information for this space.
  AllocationStats accounting_stats_;

  std::vector<ReadOnlyPage*> pages_;

  Address top_;
  Address limit_;

 private:
  // Unseal the space after it has been sealed, by making it writable.
  void Unseal();

  void DetachFromHeap() { heap_ = nullptr; }

  AllocationResult AllocateRawUnaligned(int size_in_bytes);
  AllocationResult AllocateRawAligned(int size_in_bytes,
                                      AllocationAlignment alignment);

  HeapObject TryAllocateLinearlyAligned(int size_in_bytes,
                                        AllocationAlignment alignment);
  void EnsureSpaceForAllocation(int size_in_bytes);
  void FreeLinearAllocationArea();

  // String padding must be cleared just before serialization and therefore
  // the string padding in the space will already have been cleared if the
  // space was deserialized.
  bool is_string_padding_cleared_;

  size_t capacity_;
  const size_t area_size_;
};

class SharedReadOnlySpace : public ReadOnlySpace {
 public:
  SharedReadOnlySpace(Heap* heap, std::shared_ptr<ReadOnlyArtifacts> artifacts);
  ~SharedReadOnlySpace() override;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_READ_ONLY_SPACES_H_
