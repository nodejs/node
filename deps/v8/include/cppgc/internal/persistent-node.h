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

// PersistentNode represesents a variant of two states:
// 1) traceable node with a back pointer to the Persistent object;
// 2) freelist entry.
class PersistentNode final {
 public:
  PersistentNode() = default;

  PersistentNode(const PersistentNode&) = delete;
  PersistentNode& operator=(const PersistentNode&) = delete;

  void InitializeAsUsedNode(void* owner, TraceCallback trace) {
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
    node->InitializeAsUsedNode(owner, trace);
    return node;
  }

  void FreeNode(PersistentNode* node) {
    node->InitializeAsFreeNode(free_list_head_);
    free_list_head_ = node;
  }

  void Trace(Visitor*);

  size_t NodesInUse() const;

 private:
  void EnsureNodeSlots();

  std::vector<std::unique_ptr<PersistentNodeSlots>> nodes_;
  PersistentNode* free_list_head_ = nullptr;
};

}  // namespace internal

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_PERSISTENT_NODE_H_
