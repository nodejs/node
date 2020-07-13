// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_READ_ONLY_SPACES_H_
#define V8_HEAP_READ_ONLY_SPACES_H_

#include <memory>
#include <utility>

#include "include/v8-platform.h"
#include "src/base/macros.h"
#include "src/heap/list.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

class ReadOnlyHeap;

class ReadOnlyPage : public Page {
 public:
  // Clears any pointers in the header that point out of the page that would
  // otherwise make the header non-relocatable.
  void MakeHeaderRelocatable();

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

  heap::List<MemoryChunk>& pages() { return pages_; }
  void TransferPages(heap::List<MemoryChunk>&& pages) {
    pages_ = std::move(pages);
  }

  const AllocationStats& accounting_stats() const { return stats_; }

  void set_read_only_heap(std::unique_ptr<ReadOnlyHeap> read_only_heap);
  ReadOnlyHeap* read_only_heap() { return read_only_heap_.get(); }

 private:
  heap::List<MemoryChunk> pages_;
  AllocationStats stats_;
  std::unique_ptr<SharedReadOnlySpace> shared_read_only_space_;
  std::unique_ptr<ReadOnlyHeap> read_only_heap_;
};

// -----------------------------------------------------------------------------
// Read Only space for all Immortal Immovable and Immutable objects
class ReadOnlySpace : public PagedSpace {
 public:
  explicit ReadOnlySpace(Heap* heap);

  // Detach the pages and them to artifacts for using in creating a
  // SharedReadOnlySpace.
  void DetachPagesAndAddToArtifacts(
      std::shared_ptr<ReadOnlyArtifacts> artifacts);

  ~ReadOnlySpace() override { Unseal(); }

  bool writable() const { return !is_marked_read_only_; }

  bool Contains(Address a) = delete;
  bool Contains(Object o) = delete;

  V8_EXPORT_PRIVATE void ClearStringPaddingIfNeeded();

  enum class SealMode { kDetachFromHeapAndForget, kDoNotDetachFromHeap };

  // Seal the space by marking it read-only, optionally detaching it
  // from the heap and forgetting it for memory bookkeeping purposes (e.g.
  // prevent space's memory from registering as leaked).
  void Seal(SealMode ro_mode);

  // During boot the free_space_map is created, and afterwards we may need
  // to write it into the free list nodes that were already created.
  void RepairFreeListsAfterDeserialization();

  size_t Available() override { return 0; }

 protected:
  void SetPermissionsForPages(MemoryAllocator* memory_allocator,
                              PageAllocator::Permission access);

  bool is_marked_read_only_ = false;

 private:
  // Unseal the space after is has been sealed, by making it writable.
  // TODO(v8:7464): Only possible if the space hasn't been detached.
  void Unseal();

  //
  // String padding must be cleared just before serialization and therefore the
  // string padding in the space will already have been cleared if the space was
  // deserialized.
  bool is_string_padding_cleared_;
};

class SharedReadOnlySpace : public ReadOnlySpace {
 public:
  SharedReadOnlySpace(Heap* heap, std::shared_ptr<ReadOnlyArtifacts> artifacts);
  ~SharedReadOnlySpace() override;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_READ_ONLY_SPACES_H_
