// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-pool.h"

#include <algorithm>

#include "src/base/platform/mutex.h"
#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/large-page-metadata.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/mutable-page-metadata.h"
#include "src/tasks/cancelable-task.h"

namespace v8::internal {

PooledPage::PooledPage(void* uninitialized_metadata, VirtualMemory reservation,
                       Epoch epoch)
    : uninitialized_metadata_(uninitialized_metadata),
      reservation_(std::move(reservation)),
      epoch_(epoch) {}

// static
PooledPage PooledPage::Create(PageMetadata* metadata, Epoch epoch) {
  // This method is called only on the main thread and only during the
  // atomic pause so a lock is not needed.
  DCHECK_NOT_NULL(metadata);
  DCHECK_EQ(metadata->size(), PageMetadata::kPageSize);
  DCHECK(!metadata->is_large());
  DCHECK(!metadata->is_trusted());
  DCHECK(!metadata->is_executable());
  // Ensure that ReleaseAllAllocatedMemory() was called on the page.
  DCHECK(!metadata->ContainsAnySlots());
  DCHECK(metadata->reserved_memory()->IsReserved());
  DCHECK_EQ(metadata->reserved_memory()->size(), metadata->size());

  VirtualMemory chunk_reservation(std::move(*metadata->reserved_memory()));
  DCHECK_EQ(chunk_reservation.size(), metadata->size());
  DCHECK_EQ(chunk_reservation.address(), metadata->ChunkAddress());

  MemoryChunk* chunk = metadata->Chunk();
  DCHECK(!chunk->InReadOnlySpace());

  // Destroy the chunk and the metadata object but do not release the underlying
  // memory as that is going to be pooled.
  chunk->~MemoryChunk();
  metadata->~PageMetadata();

  return PooledPage(metadata, std::move(chunk_reservation), epoch);
}

// static
PooledPage PooledPage::Create(LargePageMetadata* metadata, Epoch epoch) {
  DCHECK_NOT_NULL(metadata);
  DCHECK(metadata->is_large());
  // Ensure that ReleaseAllAllocatedMemory() was called on the page.
  DCHECK(!metadata->ContainsAnySlots());

  VirtualMemory chunk_reservation(std::move(*metadata->reserved_memory()));
  DCHECK_EQ(chunk_reservation.size(), metadata->size());
  DCHECK_EQ(chunk_reservation.address(), metadata->ChunkAddress());

  MemoryChunk* chunk = metadata->Chunk();

  // Destroy the chunk and the metadata object but do not release the underlying
  // memory as that is going to be pooled.
  chunk->~MemoryChunk();
  metadata->~LargePageMetadata();

  return PooledPage(metadata, std::move(chunk_reservation), epoch);
}

PooledPage::~PooledPage() {
  if (!reservation_.IsReserved()) {
    return;
  }
  {
    DiscardSealedMemoryScope discard_scope("Deleting MemoryChunk reservation");
    reservation_.Free();
  }
  free(uninitialized_metadata_);
}

PooledPage::Result PooledPage::ToResult() {
  base::AddressRegion uninitialized_chunk = reservation_.region();
  void* uninitialized_metadata = uninitialized_metadata_;
  // Reset the reservation without returning the memory which is now owned by
  // the caller of `Release()`.
  reservation_.Reset();
  uninitialized_metadata_ = nullptr;
  return {uninitialized_metadata, uninitialized_chunk};
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::TearDown() {
  DCHECK(local_pools_.empty());
  shared_pool_.clear();
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::PutLocal(Isolate* isolate,
                                               PoolEntry entry) {
  base::MutexGuard guard(&mutex_);
  local_pools_[isolate].emplace_back(std::move(entry));
}

template <typename PoolEntry>
std::optional<PoolEntry> MemoryPool::PoolImpl<PoolEntry>::Get(
    Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  base::MutexGuard guard(&mutex_);

  // Try to get a page from the page pool for the given isolate first.
  auto it = local_pools_.find(isolate);

  if (it == local_pools_.end()) {
    // Pages in this pool will be flushed soon. Take them first.
    if (!shared_pool_.empty()) {
      PoolEntry shared_entry = std::move(shared_pool_.back());
      shared_pool_.pop_back();
      return {std::move(shared_entry)};
    }

    // Otherwise steal from some other isolate's local pool.
    it = local_pools_.begin();
  }

  if (it != local_pools_.end()) {
    std::vector<PoolEntry>& entries = it->second;
    DCHECK(!entries.empty());
    PoolEntry local_entry = std::move(entries.back());
    entries.pop_back();
    if (entries.empty()) {
      local_pools_.erase(it);
    }
    return {std::move(local_entry)};
  }

  return std::nullopt;
}

template <typename PoolEntry>
bool MemoryPool::PoolImpl<PoolEntry>::MoveLocalToShared(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto it = local_pools_.find(isolate);
  if (it != local_pools_.end()) {
    DCHECK(!it->second.empty());
    shared_pool_.insert(shared_pool_.end(),
                        std::make_move_iterator(it->second.begin()),
                        std::make_move_iterator(it->second.end()));
    local_pools_.erase(it);
  }
  return !shared_pool_.empty();
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::ReleaseShared() {
  base::MutexGuard guard(&mutex_);
  shared_pool_.clear();
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::ReleaseLocal() {
  base::MutexGuard page_guard(&mutex_);
  local_pools_.clear();
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::ReleaseLocal(Isolate* isolate) {
  base::MutexGuard page_guard(&mutex_);
  local_pools_.erase(isolate);
}

template <typename PoolEntry>
size_t MemoryPool::PoolImpl<PoolEntry>::Size() const {
  base::MutexGuard guard(&mutex_);
  size_t count = 0;
  for (const auto& entry : local_pools_) {
    count += entry.second.size();
  }
  for (const auto& entry : shared_pool_) {
    count += entry.size();
  }
  return count;
}

template <typename PoolEntry>
size_t MemoryPool::PoolImpl<PoolEntry>::LocalSize(Isolate* isolate) const {
  base::MutexGuard guard(&mutex_);
  const auto it = local_pools_.find(isolate);
  return (it != local_pools_.end()) ? it->second.size() : 0;
}

template <typename PoolEntry>
size_t MemoryPool::PoolImpl<PoolEntry>::SharedSize() const {
  base::MutexGuard guard(&mutex_);
  return shared_pool_.size();
}

template <typename PoolEntry>
MemoryPool::PoolReleaseStats MemoryPool::PoolImpl<PoolEntry>::ReleaseUpTo(
    Epoch release_epoch) {
  std::vector<PoolEntry> entries_to_free;
  size_t freed = 0;
  bool pool_emptied = false;
  const auto collect_entries = [&entries_to_free, &freed,
                                release_epoch](PoolEntry& entry) {
    if (entry.epoch() <= release_epoch) {
      freed += entry.size();
      entries_to_free.push_back(std::move(entry));
      return true;
    }
    return false;
  };
  {
    base::MutexGuard guard(&mutex_);
    // Release shared pages of isolates that were recently torn down.
    std::erase_if(shared_pool_, collect_entries);
    // Release pooled pages of local pools.
    decltype(local_pools_) non_empty_local_pools;
    for (auto& [isolate, entries] : local_pools_) {
      std::erase_if(entries, collect_entries);
      if (!entries.empty()) {
        non_empty_local_pools.emplace(isolate, std::move(entries));
      }
    }
    std::swap(local_pools_, non_empty_local_pools);
    if (local_pools_.empty() && shared_pool_.empty()) {
      pool_emptied = true;
    }
  }
  // Entries will be freed automatically here.
  return {freed, pool_emptied};
}

bool MemoryPool::LargePagePoolImpl::Add(std::vector<LargePageMetadata*>& pages,
                                        Epoch epoch) {
  bool added_to_pool = false;
  base::MutexGuard guard(&mutex_);
  DCHECK_EQ(total_size_, ComputeTotalSize());

  const size_t max_total_size = v8_flags.max_large_page_pool_size * MB;

  std::erase_if(pages, [this, &added_to_pool, epoch,
                        max_total_size](LargePageMetadata* page) {
    if (total_size_ + page->size() > max_total_size) {
      return false;
    }

    total_size_ += page->size();
    pages_.emplace_back(PooledPage::Create(page, epoch));
    added_to_pool = true;
    return true;
  });

  DCHECK_EQ(total_size_, ComputeTotalSize());
  return added_to_pool;
}

std::optional<PooledPage::Result> MemoryPool::LargePagePoolImpl::Remove(
    Isolate* isolate, size_t chunk_size) {
  base::MutexGuard guard(&mutex_);
  auto selected = pages_.end();
  DCHECK_EQ(total_size_, ComputeTotalSize());

  // Best-fit: Select smallest large page large with a size of at least
  // |chunk_size|. In case the page is larger than necessary, the next full GC
  // will trim down its size.
  for (auto it = pages_.begin(); it != pages_.end(); it++) {
    const size_t page_size = it->size();
    if (page_size < chunk_size) {
      continue;
    }
    if (selected == pages_.end() || page_size < selected->size()) {
      selected = it;
    }
  }

  if (selected == pages_.end()) {
    return std::nullopt;
  }

  PooledPage result(std::move(*selected));
  total_size_ -= result.size();
  pages_.erase(selected);
  DCHECK_EQ(total_size_, ComputeTotalSize());
  return {result.ToResult()};
}

void MemoryPool::LargePagePoolImpl::ReleaseAll() {
  base::MutexGuard guard(&mutex_);
  pages_.clear();
  total_size_ = 0;
}

MemoryPool::PoolReleaseStats MemoryPool::LargePagePoolImpl::ReleaseUpTo(
    Epoch release_epoch) {
  std::vector<PooledPage> entries_to_free;
  bool pool_emptied = false;
  size_t freed = 0;
  {
    base::MutexGuard guard(&mutex_);
    std::erase_if(pages_, [this, &entries_to_free, release_epoch](auto& entry) {
      if (entry.epoch() <= release_epoch) {
        total_size_ -= entry.size();
        entries_to_free.push_back(std::move(entry));
        return true;
      }
      return false;
    });
    if (pages_.empty()) {
      pool_emptied = true;
    }

    DCHECK_EQ(total_size_, ComputeTotalSize());
  }
  // Entries will be freed automatically here.
  return {freed, pool_emptied};
}

size_t MemoryPool::LargePagePoolImpl::ComputeTotalSize() const {
  size_t result = 0;

  for (auto& entry : pages_) {
    result += entry.size();
  }

  return result;
}

void MemoryPool::LargePagePoolImpl::TearDown() { CHECK(pages_.empty()); }

PooledPage MemoryPool::CreatePooledPage(PageMetadata* metadata) {
  return PooledPage::Create(metadata,
                            current_epoch_.load(std::memory_order_relaxed));
}

MemoryPool::MemoryPool()
    : cancellable_task_manager_(std::make_unique<CancelableTaskManager>()) {}

MemoryPool::~MemoryPool() = default;

void MemoryPool::ReleaseOnTearDown(Isolate* isolate) {
  if (!v8_flags.memory_pool_share_memory_on_teardown) {
    ReleaseImmediately(isolate);
    return;
  }

  page_pool_.MoveLocalToShared(isolate);
  zone_pool_.MoveLocalToShared(isolate);
  large_pool_.ReleaseAll();
}

void MemoryPool::ReleaseImmediately(Isolate* isolate) {
  page_pool_.ReleaseLocal(isolate);
  zone_pool_.ReleaseLocal(isolate);
  large_pool_.ReleaseAll();
}

void MemoryPool::ReleaseAllImmediately() {
  page_pool_.ReleaseLocal();
  page_pool_.ReleaseShared();
  zone_pool_.ReleaseLocal();
  zone_pool_.ReleaseShared();
  large_pool_.ReleaseAll();
}

void MemoryPool::TearDown() {
  // Cancel all the scheduled releasing tasks first.
  cancellable_task_manager_->CancelAndWait();
  page_pool_.TearDown();
  zone_pool_.TearDown();
  large_pool_.TearDown();
}

MemoryPool::ReleaseStats MemoryPool::ReleaseUpTo(Epoch release_epoch) {
  const auto [pages_removed, normal_pools_empty] =
      page_pool_.ReleaseUpTo(release_epoch);
  const auto [large_pages_removed, large_pool_empty] =
      large_pool_.ReleaseUpTo(release_epoch);
  const auto [zone_reservations_removed, zone_pool_empty] =
      zone_pool_.ReleaseUpTo(release_epoch);
  return {.pages_removed = pages_removed,
          .large_pages_removed = large_pages_removed,
          .zone_reservations_removed = zone_reservations_removed,
          .pool_emptied =
              normal_pools_empty && large_pool_empty && zone_pool_empty};
}

size_t MemoryPool::GetCount(Isolate* isolate) const {
  return page_pool_.LocalSize(isolate);
}

size_t MemoryPool::GetSharedCount() const { return page_pool_.SharedSize(); }

size_t MemoryPool::GetTotalCount() const { return page_pool_.Size(); }

void MemoryPool::Add(Isolate* isolate, MutablePageMetadata* page) {
  DCHECK_NOT_NULL(isolate);
  page_pool_.PutLocal(
      isolate,
      PooledPage::Create(static_cast<PageMetadata*>(page),
                         current_epoch_.load(std::memory_order_relaxed)));
  PostDelayedReleaseTaskIfNeeded(isolate);
}

std::optional<PooledPage::Result> MemoryPool::Remove(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  std::optional<PooledPage> result = page_pool_.Get(isolate);
  if (!result) {
    return std::nullopt;
  }
  return result->ToResult();
}

void MemoryPool::AddLarge(Isolate* isolate,
                          std::vector<LargePageMetadata*>& pages) {
  large_pool_.Add(pages, current_epoch_.load(std::memory_order_relaxed));
  PostDelayedReleaseTaskIfNeeded(isolate);
}
std::optional<PooledPage::Result> MemoryPool::RemoveLarge(Isolate* isolate,
                                                          size_t chunk_size) {
  return large_pool_.Remove(isolate, chunk_size);
}

void MemoryPool::AddZoneReservation(Isolate* isolate,
                                    VirtualMemory zone_reservation) {
  DCHECK_NOT_NULL(isolate);
  zone_pool_.PutLocal(
      isolate,
      PooledVirtualMemory(std::move(zone_reservation),
                          current_epoch_.load(std::memory_order_relaxed)));
  PostDelayedReleaseTaskIfNeeded(isolate);
}

std::optional<VirtualMemory> MemoryPool::RemoveZoneReservation(
    Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  auto result = zone_pool_.Get(isolate);
  return result
             ? std::optional<VirtualMemory>(std::move(result->virtual_memory()))
             : std::nullopt;
}

class MemoryPool::ReleasePooledChunksTask final : public CancelableTask {
 public:
  ReleasePooledChunksTask(Isolate* isolate, MemoryPool* pool,
                          Epoch release_epoch)
      // If --single-threaded is on, attach the task to the CTM of the current
      // isolate, since the isolate may die before the task runner.
      : CancelableTask(v8_flags.single_threaded
                           ? isolate->cancelable_task_manager()
                           : pool->cancellable_task_manager_.get()),
        isolate_(isolate),
        pool_(pool),
        release_epoch_(release_epoch) {}

  ~ReleasePooledChunksTask() override = default;
  ReleasePooledChunksTask(const ReleasePooledChunksTask&) = delete;
  ReleasePooledChunksTask& operator=(const ReleasePooledChunksTask&) = delete;

 private:
  void RunInternal() override {
    const ReleaseStats stats = pool_->ReleaseUpTo(release_epoch_);
    if (v8_flags.trace_gc_nvp) {
      // Run printing on *some* isolate. The isolate_ field may be destroyed -
      // it's only used for posting a task on the single threaded configuration.
      IsolateGroup::current()->FindAnotherIsolateLocked(
          nullptr, [&stats](Isolate* isolate) {
            isolate->PrintWithTimestamp(
                "Memory pool: Removed pages: %zu, removed large pages: %zu\n, "
                "removed "
                "zone reservations: %zu\n",
                stats.pages_removed, stats.large_pages_removed,
                stats.zone_reservations_removed);
          });
    }
    // Repost itself to the next heartbeat only if pool is not fully emptied.
    if (!stats.pool_emptied) {
      pool_->PostDelayedReleaseTask(
          isolate_, base::TimeDelta::FromSeconds(v8_flags.page_pool_timeout));
    }
  }

  Isolate* const isolate_;
  MemoryPool* const pool_;
  const Epoch release_epoch_;
};

void MemoryPool::PostDelayedReleaseTask(Isolate* isolate,
                                        base::TimeDelta delay) {
  DCHECK(v8_flags.page_pool_timeout);
  // With these scheme, a pooled page may be reclaimed in [timeout, 2 * timeout)
  // second. This helps to prevent the case when a page is too prematurely
  // freed, e.g. when a GC runs right before the task is executed.
  const Epoch release_epoch =
      current_epoch_.fetch_add(1, std::memory_order_relaxed);
  auto task =
      std::make_unique<ReleasePooledChunksTask>(isolate, this, release_epoch);
  if (v8_flags.single_threaded && isolate) {
    isolate->task_runner()->PostDelayedTask(std::move(task),
                                            delay.InSecondsF());
  } else {
    V8::GetCurrentPlatform()->PostDelayedTaskOnWorkerThread(
        TaskPriority::kBestEffort, std::move(task), delay.InSecondsF());
  }
  posted_time_.store(base::TimeTicks::Now(), std::memory_order_relaxed);
}

void MemoryPool::PostDelayedReleaseTaskIfNeeded(Isolate* isolate) {
  // Allow some seconds slack if the task was not executed. This could happen if
  // the worker thread on which the task was scheduled has died.
  static constexpr base::TimeDelta kTimeSlack = base::TimeDelta::FromSeconds(4);
  const int page_pool_timeout = v8_flags.page_pool_timeout;
  if (page_pool_timeout <= 0) return;

  const base::TimeDelta delta =
      base::TimeDelta::FromSeconds(page_pool_timeout) + kTimeSlack;
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeTicks deadline =
      posted_time_.load(std::memory_order_relaxed) + delta;
  if (now > deadline) {
    PostDelayedReleaseTask(isolate, base::TimeDelta());
  }
}

void MemoryPool::CancelAndWaitForTaskToFinishForTesting() {
  cancellable_task_manager_->CancelAndWait();
}

}  // namespace v8::internal
