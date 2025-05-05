// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGE_POOL_H_
#define V8_HEAP_PAGE_POOL_H_

#include "absl/container/flat_hash_map.h"
#include "include/v8-platform.h"
#include "src/base/platform/mutex.h"

namespace v8 {
namespace internal {

class Isolate;
class MutablePageMetadata;

// Pool keeps pages allocated and accessible until explicitly flushed.
class PagePool {
 public:
  PagePool() = default;
  ~PagePool();

  PagePool(const PagePool&) = delete;
  PagePool& operator=(const PagePool&) = delete;

  // Adds page to the pool.
  void Add(Isolate* isolate, MutablePageMetadata* chunk);

  // Tries to get page from the pool. Order of priority for pools:
  //   (1) Local pool for the isolate.
  //   (2) Shared pool.
  //   (3) Steal from another isolate.
  MutablePageMetadata* Remove(Isolate* isolate);

  // Used to release the local page pool for this isolate on isolate teardown.
  // If a task can be scheduled on another isolate, freeing of pooled pages will
  // be delayed to give other isolates the chance to make use of its pooled
  // pages. If this is not possible pages will be freed immediately.
  void ReleaseOnTearDown(Isolate* isolate);

  // Releases the pooled pages immediately.
  V8_EXPORT_PRIVATE void ReleaseImmediately(Isolate* isolate);

  // Tear down this page pool. Frees all pooled pages immediately.
  void TearDown();

  // Released shared pool pages at least as old as `id`. Returns the number of
  // freed pages.
  size_t ReleaseUpTo(size_t id);

  // Returns the number of pages in the local pool for the given isolate.
  size_t GetCount(Isolate* isolate) const;

  // Returns the number of pages in the shared pool.
  size_t GetSharedCount() const;

  // Returns the number of pages cached in all local pools and the shared pool.
  size_t GetTotalCount() const;

 private:
  absl::flat_hash_map<Isolate*, std::vector<MutablePageMetadata*>> local_pools;
  // Shared pages are tracked with a logical time. This allows `ReleaseUpTo()`
  // to free all those pages where enough time has passed.
  std::vector<std::pair<std::vector<MutablePageMetadata*>, size_t>>
      shared_pool_;
  size_t next_id_ = 1;

  mutable base::Mutex mutex_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGE_POOL_H_
