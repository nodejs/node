// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_PERSISTENT_NODE_H_
#define INCLUDE_CPPGC_INTERNAL_PERSISTENT_NODE_H_

#include <array>
#include <memory>
#include <vector>

#include "cppgc/internal/logging.h"
#include "cppgc/trace-trait.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

class Visitor;

namespace internal {

class CrossThreadPersistentRegion;

// PersistentNode represents a variant of two states:
// 1) traceable node with a back pointer to the Persistent object;
// 2) freelist entry.
class PersistentNode final {
 public:
  PersistentNode() = default;

  PersistentNode(const PersistentNode&) = delete;
  PersistentNode& operator=(const PersistentNode&) = delete;

  void InitializeAsUsedNode(void* owner, TraceCallback trace) {
    CPPGC_DCHECK(trace);
    owner_ = owner;
    trace_ = trace;
  }

  void InitializeAsFreeNode(PersistentNode* next) {
    next_ = next;
    trace_ = nullptr;
  }

  void UpdateOwner(void* owner) {
    CPPGC_DCHECK(IsUsed());
    owner_ = owner;
  }

  PersistentNode* FreeListNext() const {
    CPPGC_DCHECK(!IsUsed());
    return next_;
  }

  void Trace(Visitor* visitor) const {
    CPPGC_DCHECK(IsUsed());
    trace_(visitor, owner_);
  }

  bool IsUsed() const { return trace_; }

  void* owner() const {
    CPPGC_DCHECK(IsUsed());
    return owner_;
  }

 private:
  // PersistentNode acts as a designated union:
  // If trace_ != nullptr, owner_ points to the corresponding Persistent handle.
  // Otherwise, next_ points to the next freed PersistentNode.
  union {
    void* owner_ = nullptr;
    PersistentNode* next_;
  };
  TraceCallback trace_ = nullptr;
};

class V8_EXPORT PersistentRegion final {
  using PersistentNodeSlots = std::array<PersistentNode, 256u>;

 public:
  PersistentRegion() = default;
  // Clears Persistent fields to avoid stale pointers after heap teardown.
  ~PersistentRegion();

  PersistentRegion(const PersistentRegion&) = delete;
  PersistentRegion& operator=(const PersistentRegion&) = delete;

  PersistentNode* AllocateNode(void* owner, TraceCallback trace) {
    if (!free_list_head_) {
      EnsureNodeSlots();
    }
    PersistentNode* node = free_list_head_;
    free_list_head_ = free_list_head_->FreeListNext();
    CPPGC_DCHECK(!node->IsUsed());
    node->InitializeAsUsedNode(owner, trace);
    nodes_in_use_++;
    return node;
  }

  void FreeNode(PersistentNode* node) {
    CPPGC_DCHECK(node);
    CPPGC_DCHECK(node->IsUsed());
    node->InitializeAsFreeNode(free_list_head_);
    free_list_head_ = node;
    CPPGC_DCHECK(nodes_in_use_ > 0);
    nodes_in_use_--;
  }

  void Trace(Visitor*);

  size_t NodesInUse() const;

  void ClearAllUsedNodes();

 private:
  void EnsureNodeSlots();

  std::vector<std::unique_ptr<PersistentNodeSlots>> nodes_;
  PersistentNode* free_list_head_ = nullptr;
  size_t nodes_in_use_ = 0;

  friend class CrossThreadPersistentRegion;
};

// CrossThreadPersistent uses PersistentRegion but protects it using this lock
// when needed.
class V8_EXPORT PersistentRegionLock final {
 public:
  PersistentRegionLock();
  ~PersistentRegionLock();

  static void AssertLocked();
};

// Variant of PersistentRegion that checks whether the PersistentRegionLock is
// locked.
class V8_EXPORT CrossThreadPersistentRegion final {
 public:
  CrossThreadPersistentRegion() = default;
  // Clears Persistent fields to avoid stale pointers after heap teardown.
  ~CrossThreadPersistentRegion();

  CrossThreadPersistentRegion(const CrossThreadPersistentRegion&) = delete;
  CrossThreadPersistentRegion& operator=(const CrossThreadPersistentRegion&) =
      delete;

  V8_INLINE PersistentNode* AllocateNode(void* owner, TraceCallback trace) {
    PersistentRegionLock::AssertLocked();
    return persistent_region_.AllocateNode(owner, trace);
  }

  V8_INLINE void FreeNode(PersistentNode* node) {
    PersistentRegionLock::AssertLocked();
    persistent_region_.FreeNode(node);
  }

  void Trace(Visitor*);

  size_t NodesInUse() const;

  void ClearAllUsedNodes();

 private:
  PersistentRegion persistent_region_;
};

}  // namespace internal

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_PERSISTENT_NODE_H_
