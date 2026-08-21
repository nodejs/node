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

PooledPage::PooledPage(void* uninitialized_metadata, VirtualMemory reservation)
    : uninitialized_metadata_(uninitialized_metadata),
      reservation_(std::move(reservation)) {}

// static
PooledPage PooledPage::Create(PageMetadata* metadata) {
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

  return PooledPage(metadata, std::move(chunk_reservation));
}

// static
PooledPage PooledPage::Create(LargePageMetadata* metadata) {
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

  return PooledPage(metadata, std::move(chunk_reservation));
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
      auto& shared_entries = shared_pool_.back().second;
      if (!shared_entries.empty()) {
        PoolEntry shared_entry = std::move(shared_entries.back());
        shared_entries.pop_back();
        if (shared_entries.empty()) {
          shared_pool_.pop_back();
        }
        return {std::move(shared_entry)};
      }
    }

    // Otherwise steal from some other isolate's local pool.
    it = local_pools_.begin();
  }

  if (it != local_pools_.end()) {
    DCHECK(!it->second.empty());
    std::vector<PoolEntry>& entries = it->second;
    if (!entries.empty()) {
      PoolEntry local_entry = std::move(entries.back());
      entries.pop_back();
      if (entries.empty()) {
        local_pools_.erase(it);
      }
      return {std::move(local_entry)};
    }
  }

  return std::nullopt;
}

template <typename PoolEntry>
bool MemoryPool::PoolImpl<PoolEntry>::MoveLocalToShared(
    Isolate* isolate, InternalTime release_time) {
  base::MutexGuard guard(&mutex_);
  auto it = local_pools_.find(isolate);
  if (it != local_pools_.end()) {
    DCHECK(!it->second.empty());
    shared_pool_.push_back(std::make_pair(release_time, std::move(it->second)));
    local_pools_.erase(it);
  }
  return !shared_pool_.empty();
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::ReleaseShared() {
  std::vector<std::pair<InternalTime, std::vector<PoolEntry>>> entries_to_free;
  {
    base::MutexGuard guard(&mutex_);
    entries_to_free = std::move(shared_pool_);
    shared_pool_.clear();
  }
  // Entries will be freed automatically here.
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::ReleaseLocal() {
  std::vector<std::vector<PoolEntry>> entries_to_free;
  {
    base::MutexGuard page_guard(&mutex_);
    for (auto& entry : local_pools_) {
      entries_to_free.push_back(std::move(entry.second));
    }
    local_pools_.clear();
  }
  // Entries will be freed automatically here.
}

template <typename PoolEntry>
void MemoryPool::PoolImpl<PoolEntry>::ReleaseLocal(Isolate* isolate) {
  std::vector<PoolEntry> entries_to_free;
  {
    base::MutexGuard page_guard(&mutex_);
    const auto it = local_pools_.find(isolate);
    if (it != local_pools_.end()) {
      DCHECK(!it->second.empty());
      entries_to_free = std::move(it->second);
      local_pools_.erase(it);
    }
  }
  // Entries will be freed automatically here.
}

template <typename PoolEntry>
size_t MemoryPool::PoolImpl<PoolEntry>::Size() const {
  base::MutexGuard guard(&mutex_);
  size_t count = 0;
  for (const auto& entry : local_pools_) {
    count += entry.second.size();
  }
  for (const auto& entry : shared_pool_) {
    count += entry.second.size();
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
  size_t count = 0;
  for (const auto& entry : shared_pool_) {
    count += entry.second.size();
  }
  return count;
}

template <typename PoolEntry>
size_t MemoryPool::PoolImpl<PoolEntry>::ReleaseUpTo(InternalTime release_time) {
  std::vector<std::vector<PoolEntry>> entries_to_free;
  size_t freed = 0;
  {
    base::MutexGuard guard(&mutex_);
    std::erase_if(shared_pool_,
                  [&entries_to_free, &freed, release_time](auto& entry) {
                    if (entry.first <= release_time) {
                      freed += entry.second.size();
                      entries_to_free.push_back(std::move(entry.second));
                      return true;
                    }
                    return false;
                  });
  }
  // Entries will be freed automatically here.
  return freed;
}

bool MemoryPool::LargePagePoolImpl::Add(std::vector<LargePageMetadata*>& pages,
                                        InternalTime time) {
  bool added_to_pool = false;
  base::MutexGuard guard(&mutex_);
  DCHECK_EQ(total_size_, ComputeTotalSize());

  const size_t max_total_size = v8_flags.max_large_page_pool_size * MB;

  std::erase_if(pages, [this, &added_to_pool, time,
                        max_total_size](LargePageMetadata* page) {
    if (total_size_ + page->size() > max_total_size) {
      return false;
    }

    total_size_ += page->size();
    pages_.emplace_back(time, PooledPage::Create(page));
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
    const size_t page_size = it->second.size();
    if (page_size < chunk_size) {
      continue;
    }
    if (selected == pages_.end() || page_size < selected->second.size()) {
      selected = it;
    }
  }

  if (selected == pages_.end()) {
    return std::nullopt;
  }

  PooledPage result(std::move(selected->second));
  total_size_ -= result.size();
  pages_.erase(selected);
  DCHECK_EQ(total_size_, ComputeTotalSize());
  return {result.ToResult()};
}

void MemoryPool::LargePagePoolImpl::ReleaseAll() {
  std::vector<std::pair<InternalTime, PooledPage>> pages_to_free;

  {
    base::MutexGuard guard(&mutex_);
    pages_to_free = std::move(pages_);
    total_size_ = 0;
  }

  // Entries will be freed automatically here.
}

size_t MemoryPool::LargePagePoolImpl::ReleaseUpTo(InternalTime release_time) {
  std::vector<PooledPage> entries_to_free;
  size_t freed = 0;
  {
    base::MutexGuard guard(&mutex_);
    std::erase_if(pages_, [this, &entries_to_free, release_time](auto& entry) {
      if (entry.first <= release_time) {
        total_size_ -= entry.second.size();
        entries_to_free.push_back(std::move(entry.second));
        return true;
      }
      return false;
    });

    DCHECK_EQ(total_size_, ComputeTotalSize());
  }
  // Entries will be freed automatically here.
  return freed;
}

size_t MemoryPool::LargePagePoolImpl::ComputeTotalSize() const {
  size_t result = 0;

  for (auto& entry : pages_) {
    result += entry.second.size();
  }

  return result;
}

void MemoryPool::LargePagePoolImpl::TearDown() { CHECK(pages_.empty()); }

MemoryPool::~MemoryPool() = default;

class MemoryPool::ReleasePooledChunksTask final : public CancelableTask {
 public:
  ReleasePooledChunksTask(Isolate* isolate, MemoryPool* pool,
                          InternalTime release_time)
      : CancelableTask(isolate),
        isolate_(isolate),
        pool_(pool),
        release_time_(release_time) {}

  ~ReleasePooledChunksTask() override = default;
  ReleasePooledChunksTask(const ReleasePooledChunksTask&) = delete;
  ReleasePooledChunksTask& operator=(const ReleasePooledChunksTask&) = delete;

 private:
  void RunInternal() override {
    PtrComprCageAccessScope scope(isolate_);
    pool_->ReleaseUpTo(isolate_, release_time_);
  }

  Isolate* const isolate_;
  MemoryPool* const pool_;
  const InternalTime release_time_;
};

void MemoryPool::ReleaseOnTearDown(Isolate* isolate) {
  if (!v8_flags.memory_pool_share_memory_on_teardown) {
    ReleaseImmediately(isolate);
    return;
  }

  const InternalTime time = next_time_.fetch_add(1, std::memory_order_relaxed);

  const bool shared_page_pool_populate =
      page_pool_.MoveLocalToShared(isolate, time);
  const bool shared_zone_pool_populated =
      zone_pool_.MoveLocalToShared(isolate, time);

  // Always post task when there are pages in the shared pool.
  if (shared_page_pool_populate || shared_zone_pool_populated) {
    auto schedule_task = [this, isolate, time](Isolate* target_isolate) {
      DCHECK_NE(isolate, target_isolate);
      USE(isolate);
      static constexpr base::TimeDelta kReleaseTaskDelayInSeconds =
          base::TimeDelta::FromSeconds(8);
      target_isolate->task_runner()->PostDelayedTask(
          std::make_unique<ReleasePooledChunksTask>(target_isolate, this, time),
          kReleaseTaskDelayInSeconds.InSecondsF());
    };

    if (!isolate->isolate_group()->FindAnotherIsolateLocked(isolate,
                                                            schedule_task)) {
      // No other isolate could be found. Release pooled pages right away.
      page_pool_.ReleaseShared();
      zone_pool_.ReleaseShared();
    }
  }

  large_pool_.ReleaseAll();
}

void MemoryPool::ReleaseImmediately(Isolate* isolate) {
  page_pool_.ReleaseLocal(isolate);
  zone_pool_.ReleaseLocal(isolate);
  large_pool_.ReleaseAll();
}

void MemoryPool::ReleaseLargeImmediately() { large_pool_.ReleaseAll(); }

void MemoryPool::ReleaseAllImmediately() {
  page_pool_.ReleaseLocal();
  page_pool_.ReleaseShared();
  zone_pool_.ReleaseLocal();
  zone_pool_.ReleaseShared();
  large_pool_.ReleaseAll();
}

void MemoryPool::TearDown() {
  page_pool_.TearDown();
  zone_pool_.TearDown();
  large_pool_.TearDown();
}

void MemoryPool::ReleaseUpTo(Isolate* isolate_for_printing,
                             InternalTime release_time) {
  const auto pages_removed = page_pool_.ReleaseUpTo(release_time);
  const auto zone_reservations_removed = zone_pool_.ReleaseUpTo(release_time);
  if (v8_flags.trace_gc_nvp) {
    isolate_for_printing->PrintWithTimestamp(
        "Shared pool: Removed pages: %zu removed zone reservations: %zu\n",
        pages_removed, zone_reservations_removed);
  }
}

size_t MemoryPool::GetCount(Isolate* isolate) const {
  return page_pool_.LocalSize(isolate);
}

size_t MemoryPool::GetSharedCount() const { return page_pool_.SharedSize(); }

size_t MemoryPool::GetTotalCount() const { return page_pool_.Size(); }

void MemoryPool::Add(Isolate* isolate, MutablePageMetadata* page) {
  DCHECK_NOT_NULL(isolate);
  page_pool_.PutLocal(isolate,
                      PooledPage::Create(static_cast<PageMetadata*>(page)));
}

std::optional<PooledPage::Result> MemoryPool::Remove(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  std::optional<PooledPage> result = page_pool_.Get(isolate);
  if (!result) {
    return std::nullopt;
  }
  return result->ToResult();
}

class MemoryPool::ReleasePooledLargeChunksTask final : public CancelableTask {
 public:
  ReleasePooledLargeChunksTask(Isolate* isolate, MemoryPool* pool,
                               InternalTime time)
      : CancelableTask(isolate), isolate_(isolate), pool_(pool), time_(time) {}

  ~ReleasePooledLargeChunksTask() override = default;
  ReleasePooledLargeChunksTask(const ReleasePooledLargeChunksTask&) = delete;
  ReleasePooledLargeChunksTask& operator=(const ReleasePooledLargeChunksTask&) =
      delete;

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override {
    PtrComprCageAccessScope scope(isolate_);
    pool_->large_pool_.ReleaseUpTo(time_);
  }

  Isolate* const isolate_;
  MemoryPool* pool_;
  InternalTime time_;
};

void MemoryPool::AddLarge(Isolate* isolate,
                          std::vector<LargePageMetadata*>& pages) {
  const InternalTime time = next_time_.fetch_add(1, std::memory_order_relaxed);
  const bool added_to_pool = large_pool_.Add(pages, time);
  const int timeout = v8_flags.large_page_pool_timeout;
  if (added_to_pool && timeout > 0) {
    const base::TimeDelta large_page_release_task_delay =
        base::TimeDelta::FromSeconds(timeout);
    auto task =
        std::make_unique<ReleasePooledLargeChunksTask>(isolate, this, time);
    if (v8_flags.single_threaded) {
      isolate->task_runner()->PostDelayedTask(
          std::move(task), large_page_release_task_delay.InSecondsF());
    } else {
      V8::GetCurrentPlatform()->PostDelayedTaskOnWorkerThread(
          TaskPriority::kBestEffort, std::move(task),
          large_page_release_task_delay.InSecondsF());
    }
  }
}

std::optional<PooledPage::Result> MemoryPool::RemoveLarge(Isolate* isolate,
                                                          size_t chunk_size) {
  return large_pool_.Remove(isolate, chunk_size);
}

void MemoryPool::AddZoneReservation(Isolate* isolate,
                                    VirtualMemory zone_reservation) {
  DCHECK_NOT_NULL(isolate);
  zone_pool_.PutLocal(isolate, std::move(zone_reservation));
}

std::optional<VirtualMemory> MemoryPool::RemoveZoneReservation(
    Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  return zone_pool_.Get(isolate);
}

}  // namespace v8::internal
