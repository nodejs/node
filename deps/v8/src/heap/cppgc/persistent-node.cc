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

PersistentRegion::~PersistentRegion() { ClearAllUsedNodes(); }

void PersistentRegion::ClearAllUsedNodes() {
  for (auto& slots : nodes_) {
    for (auto& node : *slots) {
      if (node.IsUsed()) {
        static_cast<PersistentBase*>(node.owner())->ClearFromGC();
        // Add nodes back to the free list to allow reusing for subsequent
        // creation calls.
        node.InitializeAsFreeNode(free_list_head_);
        free_list_head_ = &node;
        CPPGC_DCHECK(nodes_in_use_ > 0);
        nodes_in_use_--;
      }
    }
  }
  CPPGC_DCHECK(0u == nodes_in_use_);
}

size_t PersistentRegion::NodesInUse() const {
#ifdef DEBUG
  const size_t accumulated_nodes_in_use_ = std::accumulate(
      nodes_.cbegin(), nodes_.cend(), 0u, [](size_t acc, const auto& slots) {
        return acc + std::count_if(slots->cbegin(), slots->cend(),
                                   [](const PersistentNode& node) {
                                     return node.IsUsed();
                                   });
      });
  DCHECK_EQ(accumulated_nodes_in_use_, nodes_in_use_);
#endif  // DEBUG
  return nodes_in_use_;
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

// static
void PersistentRegionLock::AssertLocked() {
  return g_process_mutex.Pointer()->AssertHeld();
}

CrossThreadPersistentRegion::~CrossThreadPersistentRegion() {
  PersistentRegionLock guard;
  persistent_region_.ClearAllUsedNodes();
  persistent_region_.nodes_.clear();
}

void CrossThreadPersistentRegion::Trace(Visitor* visitor) {
  PersistentRegionLock::AssertLocked();
  return persistent_region_.Trace(visitor);
}

size_t CrossThreadPersistentRegion::NodesInUse() const {
  // This method does not require a lock.
  return persistent_region_.NodesInUse();
}

void CrossThreadPersistentRegion::ClearAllUsedNodes() {
  PersistentRegionLock::AssertLocked();
  return persistent_region_.ClearAllUsedNodes();
}

}  // namespace internal
}  // namespace cppgc
