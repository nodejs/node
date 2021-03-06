// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/persistent-node.h"

#include <algorithm>
#include <numeric>

#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/process-heap.h"

namespace cppgc {
namespace internal {

PersistentRegion::~PersistentRegion() {
  for (auto& slots : nodes_) {
    for (auto& node : *slots) {
      if (node.IsUsed()) {
        static_cast<PersistentBase*>(node.owner())->ClearFromGC();
      }
    }
  }
}

size_t PersistentRegion::NodesInUse() const {
  return std::accumulate(
      nodes_.cbegin(), nodes_.cend(), 0u, [](size_t acc, const auto& slots) {
        return acc + std::count_if(slots->cbegin(), slots->cend(),
                                   [](const PersistentNode& node) {
                                     return node.IsUsed();
                                   });
      });
}

void PersistentRegion::EnsureNodeSlots() {
  nodes_.push_back(std::make_unique<PersistentNodeSlots>());
  for (auto& node : *nodes_.back()) {
    node.InitializeAsFreeNode(free_list_head_);
    free_list_head_ = &node;
  }
}

void PersistentRegion::Trace(Visitor* visitor) {
  free_list_head_ = nullptr;
  for (auto& slots : nodes_) {
    bool is_empty = true;
    for (auto& node : *slots) {
      if (node.IsUsed()) {
        node.Trace(visitor);
        is_empty = false;
      } else {
        node.InitializeAsFreeNode(free_list_head_);
        free_list_head_ = &node;
      }
    }
    if (is_empty) {
      PersistentNode* first_next = (*slots)[0].FreeListNext();
      // First next was processed first in the loop above, guaranteeing that it
      // either points to null or into a different node block.
      CPPGC_DCHECK(!first_next || first_next < &slots->front() ||
                   first_next > &slots->back());
      free_list_head_ = first_next;
      slots.reset();
    }
  }
  nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
                              [](const auto& ptr) { return !ptr; }),
               nodes_.end());
}

PersistentRegionLock::PersistentRegionLock() {
  g_process_mutex.Pointer()->Lock();
}

PersistentRegionLock::~PersistentRegionLock() {
  g_process_mutex.Pointer()->Unlock();
}

}  // namespace internal
}  // namespace cppgc
