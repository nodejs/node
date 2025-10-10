// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_POOL_H_
#define V8_HEAP_MEMORY_POOL_H_

#include "absl/container/flat_hash_map.h"
#include "src/base/platform/mutex.h"
#include "src/utils/allocation.h"

namespace v8::internal {

class Isolate;
class LargePageMetadata;
class MutablePageMetadata;

// Pool that keeps memory cached until explicitly flushed. The pool assumes that
// memory can be freely reused and globally shared. E.g., pages entering the
// pool can immediately be reused by some other Isolate.
//
// Currently used for pages and zone reservations.
class MemoryPool final {
  // Logical time used to indicate when memory is supposed to be released.
  using InternalTime = size_t;

 public:
  MemoryPool() = default;
  ~MemoryPool();

  MemoryPool(const MemoryPool&) = delete;
  MemoryPool& operator=(const MemoryPool&) = delete;

  // Adds page to the pool.
  void Add(Isolate* isolate, MutablePageMetadata* chunk);

  // Tries to get page from the pool. Order of priority for pools:
  // 1. Local pool for the isolate.
  // 2. Shared pool.
  // 3. Steal from another isolate.
  MutablePageMetadata* Remove(Isolate* isolate);

  void AddLarge(Isolate* isolate, std::vector<LargePageMetadata*>& pages);
  LargePageMetadata* RemoveLarge(Isolate* isolate, size_t chunk_size);

  // Adds a zone reservation to the pool.
  void AddZoneReservation(Isolate* isolate, VirtualMemory zone_reservation);

  // Tries to get a zone reservation from the pool. See Remove() for details.
  std::optional<VirtualMemory> RemoveZoneReservation(Isolate* isolate);

  // Used to release the local page pool for this isolate on isolate teardown.
  // If a task can be scheduled on another isolate, freeing of pooled pages will
  // be delayed to give other isolates the chance to make use of its pooled
  // pages. If this is not possible pages will be freed immediately.
  void ReleaseOnTearDown(Isolate* isolate);

  // Releases the pooled pages of a specific isolate immediately.
  V8_EXPORT_PRIVATE void ReleaseImmediately(Isolate* isolate);

  // Releases large poold pages immediately.
  void ReleaseLargeImmediately();

  // Releases all the pooled pages immediately.
  void ReleaseAllImmediately();

  // Tear down this page pool. Frees all pooled pages immediately.
  void TearDown();

  // Notifies the pool that a GC is going to start.
  void GarbageCollectionPrologue(Isolate* isolate, GarbageCollector collector);

  // Returns the number of pages in the local pool for the given isolate.
  size_t GetCount(Isolate* isolate) const;

  // Returns the number of pages in the shared pool.
  size_t GetSharedCount() const;

  // Returns the number of pages cached in all local pools and the shared pool.
  size_t GetTotalCount() const;

 private:
  class ReleasePooledChunksTask;
  class ReleasePooledLargeChunksTask;

  // Simple wrapper to be able to free the regular page via destructors.
  using PageMemory = std::unique_ptr<MutablePageMetadata,
                                     std::function<void(MutablePageMetadata*)>>;
  // Simple wrapper to be able to free the large page via destructors.
  using LargePageMemory =
      std::unique_ptr<LargePageMetadata,
                      std::function<void(LargePageMetadata*)>>;

  template <typename PoolEntry>
  class PoolImpl final {
   public:
    ~PoolImpl() {
      DCHECK(local_pools_.empty());
      DCHECK(shared_pool_.empty());
    }
    void PutLocal(Isolate* isolate, PoolEntry entry);
    std::optional<PoolEntry> Get(Isolate* isolate);
    bool MoveLocalToShared(Isolate* isolate, InternalTime release_time);
    void ReleaseShared();
    void ReleaseLocal();
    void ReleaseLocal(Isolate* isolate);
    size_t ReleaseUpTo(InternalTime release_time);

    void TearDown();

    size_t Size() const;
    size_t LocalSize(Isolate* isolate) const;
    size_t SharedSize() const;

   private:
    absl::flat_hash_map<Isolate*, std::vector<PoolEntry>> local_pools_;
    // Shared pages are tracked with a logical time. This allows `ReleaseUpTo()`
    // to free all those pages where enough time has passed.
    std::vector<std::pair<InternalTime, std::vector<PoolEntry>>> shared_pool_;
    mutable base::Mutex mutex_;
  };

  class LargePagePoolImpl final {
   public:
    ~LargePagePoolImpl() { DCHECK(pages_.empty()); }

    bool Add(std::vector<LargePageMetadata*>& pages, InternalTime time);
    LargePageMetadata* Remove(Isolate* isolate, size_t chunk_size);
    void ReleaseAll();
    size_t ReleaseUpTo(InternalTime release_time);

    void TearDown();
    size_t ComputeTotalSize() const;

   private:
    base::Mutex mutex_;
    std::vector<std::pair<InternalTime, LargePageMemory>> pages_;
    size_t total_size_ = 0;
  };

  PoolImpl<PageMemory> page_pool_;
  PoolImpl<VirtualMemory> zone_pool_;

  LargePagePoolImpl large_pool_;

  // Released shared pool pages at least as old as `release_time`. Returns the
  // number of freed pages.
  void ReleaseUpTo(Isolate* isolate_for_printing, InternalTime release_time);

  std::atomic<InternalTime> next_time_{1};
};

}  // namespace v8::internal

#endif  // V8_HEAP_MEMORY_POOL_H_
