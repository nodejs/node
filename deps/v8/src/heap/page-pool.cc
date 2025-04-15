// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/page-pool.h"

#include "src/execution/isolate.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/mutable-page-metadata.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

PagePool::~PagePool() {
  DCHECK(shared_pool_.empty());
  DCHECK(local_pools.empty());
}

class ReleasePooledChunksTask final : public CancelableTask {
 public:
  ReleasePooledChunksTask(Isolate* isolate, PagePool* pool, size_t id)
      : CancelableTask(isolate), isolate_(isolate), pool_(pool), id_(id) {}

  ~ReleasePooledChunksTask() override = default;
  ReleasePooledChunksTask(const ReleasePooledChunksTask&) = delete;
  ReleasePooledChunksTask& operator=(const ReleasePooledChunksTask&) = delete;

 private:
  void RunInternal() override {
    size_t count = pool_->ReleaseUpTo(id_);
    if (v8_flags.trace_gc_nvp) {
      isolate_->PrintWithTimestamp("Removed %zu pages from the shared pool.\n",
                                   count);
    }
  }

  Isolate* const isolate_;
  PagePool* const pool_;
  const size_t id_;
};

void PagePool::ReleaseOnTearDown(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);
  auto it = local_pools.find(isolate);

  size_t id = next_id_++;

  if (it != local_pools.end()) {
    DCHECK(!it->second.empty());
    shared_pool_.push_back(std::make_pair(std::move(it->second), id));
    local_pools.erase(it);
  }

  // Always post task when there are pages in the shared pool.
  if (!shared_pool_.empty()) {
    auto schedule_task = [this, isolate, id](Isolate* target_isolate) {
      DCHECK_NE(isolate, target_isolate);
      USE(isolate);
      static constexpr base::TimeDelta kReleaseTaskDelayInSeconds =
          base::TimeDelta::FromSeconds(8);
      target_isolate->task_runner()->PostDelayedTask(
          std::make_unique<ReleasePooledChunksTask>(target_isolate, this, id),
          kReleaseTaskDelayInSeconds.InSecondsF());
    };

    if (!isolate->isolate_group()->FindAnotherIsolateLocked(isolate,
                                                            schedule_task)) {
      // No other isolate could be found. Release pooled pages right away.
      for (auto entry : shared_pool_) {
        for (auto page : entry.first) {
          MemoryAllocator::DeleteMemoryChunk(page);
        }
      }
      shared_pool_.clear();
    }
  }
}

void PagePool::ReleaseImmediately(Isolate* isolate) {
  std::vector<MutablePageMetadata*> pages_to_free;

  {
    base::MutexGuard guard(&mutex_);
    auto it = local_pools.find(isolate);

    if (it != local_pools.end()) {
      DCHECK(!it->second.empty());
      pages_to_free = std::move(it->second);
      local_pools.erase(it);
    }
  }

  for (MutablePageMetadata* page : pages_to_free) {
    MemoryAllocator::DeleteMemoryChunk(page);
  }
}

void PagePool::TearDown() {
  DCHECK(local_pools.empty());

  for (auto entry : shared_pool_) {
    for (auto page : entry.first) {
      MemoryAllocator::DeleteMemoryChunk(page);
    }
  }
  shared_pool_.clear();
}

size_t PagePool::ReleaseUpTo(size_t id) {
  std::vector<std::vector<MutablePageMetadata*>> groups_to_free;

  {
    base::MutexGuard guard(&mutex_);
    std::erase_if(shared_pool_, [&groups_to_free, id](auto entry) {
      if (entry.second <= id) {
        groups_to_free.push_back(std::move(entry.first));
        return true;
      }

      return false;
    });
  }

  size_t freed = 0;

  for (auto group : groups_to_free) {
    for (auto page : group) {
      MemoryAllocator::DeleteMemoryChunk(page);
      ++freed;
    }
  }

  return freed;
}

size_t PagePool::GetCount(Isolate* isolate) const {
  base::MutexGuard guard(&mutex_);
  auto it = local_pools.find(isolate);
  if (it != local_pools.end()) {
    return it->second.size();
  }

  return 0;
}

size_t PagePool::GetSharedCount() const {
  base::MutexGuard guard(&mutex_);
  size_t count = 0;

  for (auto& entry : shared_pool_) {
    count += entry.first.size();
  }

  return count;
}

size_t PagePool::GetTotalCount() const {
  base::MutexGuard guard(&mutex_);
  size_t count = 0;

  for (auto& entry : shared_pool_) {
    count += entry.first.size();
  }

  for (auto& entry : local_pools) {
    count += entry.second.size();
  }

  return count;
}

void PagePool::Add(Isolate* isolate, MutablePageMetadata* chunk) {
  // This method is called only on the main thread and only during the
  // atomic pause so a lock is not needed.
  DCHECK_NOT_NULL(chunk);
  DCHECK_EQ(chunk->size(), PageMetadata::kPageSize);
  DCHECK(!chunk->Chunk()->IsLargePage());
  DCHECK(!chunk->Chunk()->IsTrusted());
  DCHECK_NE(chunk->Chunk()->executable(), EXECUTABLE);
  chunk->ReleaseAllAllocatedMemory();
  base::MutexGuard guard(&mutex_);
  local_pools[isolate].push_back(chunk);
}

MutablePageMetadata* PagePool::Remove(Isolate* isolate) {
  base::MutexGuard guard(&mutex_);

  // Try to get a page from the page pool for the given isolate first.
  auto it = local_pools.find(isolate);

  if (it == local_pools.end()) {
    // Pages in this pool will be flushed soon. Take them first.
    if (!shared_pool_.empty()) {
      auto& group = shared_pool_.back().first;
      MutablePageMetadata* page = group.back();
      group.pop_back();
      if (group.empty()) {
        shared_pool_.pop_back();
      }
      return page;
    }

    // Otherwise steal from some other isolate's local pool.
    it = local_pools.begin();
  }

  if (it != local_pools.end()) {
    std::vector<MutablePageMetadata*>& pages = it->second;
    DCHECK(!pages.empty());

    MutablePageMetadata* result = pages.back();
    pages.pop_back();

    if (pages.empty()) {
      local_pools.erase(it);
    }

    return result;
  }

  return nullptr;
}

}  // namespace internal
}  // namespace v8
