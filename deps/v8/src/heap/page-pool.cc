// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/page-pool.h"

#include "src/execution/isolate.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/mutable-page-metadata.h"
#include "src/tasks/cancelable-task.h"

namespace v8::internal {

template <typename PoolEntry>
void PagePool::PoolImpl<PoolEntry>::TearDown() {
  DCHECK(local_pools_.empty());
  shared_pool_.clear();
}

template <typename PoolEntry>
void PagePool::PoolImpl<PoolEntry>::PutLocal(Isolate* isolate,
                                             PoolEntry entry) {
  base::MutexGuard guard(&mutex_);
  local_pools_[isolate].emplace_back(std::move(entry));
}

template <typename PoolEntry>
std::optional<PoolEntry> PagePool::PoolImpl<PoolEntry>::Get(Isolate* isolate) {
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
bool PagePool::PoolImpl<PoolEntry>::MoveLocalToShared(
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
void PagePool::PoolImpl<PoolEntry>::ClearShared() {
  std::vector<std::pair<InternalTime, std::vector<PoolEntry>>> entries_to_free;
  {
    base::MutexGuard guard(&mutex_);
    entries_to_free = std::move(shared_pool_);
    shared_pool_.clear();
  }
  // Entries will be freed automatically here.
}

template <typename PoolEntry>
void PagePool::PoolImpl<PoolEntry>::ClearLocal() {
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
void PagePool::PoolImpl<PoolEntry>::ClearLocal(Isolate* isolate) {
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
size_t PagePool::PoolImpl<PoolEntry>::Size() const {
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
size_t PagePool::PoolImpl<PoolEntry>::LocalSize(Isolate* isolate) const {
  base::MutexGuard guard(&mutex_);
  const auto it = local_pools_.find(isolate);
  return (it != local_pools_.end()) ? it->second.size() : 0;
}

template <typename PoolEntry>
size_t PagePool::PoolImpl<PoolEntry>::SharedSize() const {
  base::MutexGuard guard(&mutex_);
  size_t count = 0;
  for (const auto& entry : shared_pool_) {
    count += entry.second.size();
  }
  return count;
}

template <typename PoolEntry>
size_t PagePool::PoolImpl<PoolEntry>::ReleaseUpTo(InternalTime release_time) {
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

PagePool::~PagePool() = default;

class PagePool::ReleasePooledChunksTask final : public CancelableTask {
 public:
  ReleasePooledChunksTask(Isolate* isolate, PagePool* pool,
                          InternalTime release_time)
      : CancelableTask(isolate),
        isolate_(isolate),
        pool_(pool),
        release_time_(release_time) {}

  ~ReleasePooledChunksTask() override = default;
  ReleasePooledChunksTask(const ReleasePooledChunksTask&) = delete;
  ReleasePooledChunksTask& operator=(const ReleasePooledChunksTask&) = delete;

 private:
  void RunInternal() override { pool_->ReleaseUpTo(isolate_, release_time_); }

  Isolate* const isolate_;
  PagePool* const pool_;
  const InternalTime release_time_;
};

void PagePool::ReleaseOnTearDown(Isolate* isolate) {
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
      page_pool_.ClearShared();
      zone_pool_.ClearShared();
    }
  }
}

void PagePool::ReleaseImmediately(Isolate* isolate) {
  page_pool_.ClearLocal(isolate);
  zone_pool_.ClearLocal(isolate);
}

void PagePool::ReleaseImmediately() {
  page_pool_.ClearLocal();
  page_pool_.ClearShared();
  zone_pool_.ClearLocal();
  page_pool_.ClearShared();
}

void PagePool::TearDown() {
  page_pool_.TearDown();
  zone_pool_.TearDown();
}

void PagePool::ReleaseUpTo(Isolate* isolate_for_printing,
                           InternalTime release_time) {
  const auto pages_removed = page_pool_.ReleaseUpTo(release_time);
  const auto zone_reservations_removed = zone_pool_.ReleaseUpTo(release_time);
  if (v8_flags.trace_gc_nvp) {
    isolate_for_printing->PrintWithTimestamp(
        "Shared pool: Removed pages: %zu removed zone reservations: %zu\n",
        pages_removed, zone_reservations_removed);
  }
}

size_t PagePool::GetCount(Isolate* isolate) const {
  return page_pool_.LocalSize(isolate);
}

size_t PagePool::GetSharedCount() const { return page_pool_.SharedSize(); }

size_t PagePool::GetTotalCount() const { return page_pool_.Size(); }

void PagePool::Add(Isolate* isolate, MutablePageMetadata* chunk) {
  DCHECK_NOT_NULL(isolate);
  // This method is called only on the main thread and only during the
  // atomic pause so a lock is not needed.
  DCHECK_NOT_NULL(chunk);
  DCHECK_EQ(chunk->size(), PageMetadata::kPageSize);
  DCHECK(!chunk->Chunk()->IsLargePage());
  DCHECK(!chunk->Chunk()->IsTrusted());
  DCHECK_NE(chunk->Chunk()->executable(), EXECUTABLE);
  chunk->ReleaseAllAllocatedMemory();
  page_pool_.PutLocal(isolate,
                      PageMemory(chunk, [](MutablePageMetadata* metadata) {
                        MemoryAllocator::DeleteMemoryChunk(metadata);
                      }));
}

MutablePageMetadata* PagePool::Remove(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  auto result = page_pool_.Get(isolate);
  if (result) {
    return result.value().release();
  }
  return nullptr;
}

void PagePool::AddZoneReservation(Isolate* isolate,
                                  VirtualMemory zone_reservation) {
  DCHECK_NOT_NULL(isolate);
  zone_pool_.PutLocal(isolate, std::move(zone_reservation));
}

std::optional<VirtualMemory> PagePool::RemoveZoneReservation(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  return zone_pool_.Get(isolate);
}

}  // namespace v8::internal
