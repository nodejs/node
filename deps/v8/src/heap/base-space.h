// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_SPACE_H_
#define V8_HEAP_BASE_SPACE_H_

#include <atomic>

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/heap/heap-verifier.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class Heap;

// ----------------------------------------------------------------------------
// BaseSpace is the abstract superclass for all allocation spaces.
class V8_EXPORT_PRIVATE BaseSpace : public Malloced {
 public:
  BaseSpace(const BaseSpace&) = delete;
  BaseSpace& operator=(const BaseSpace&) = delete;

  Heap* heap() const {
    DCHECK_NOT_NULL(heap_);
    return heap_;
  }

  AllocationSpace identity() const { return id_; }

  // Return the total amount committed memory for this space, i.e., allocatable
  // memory and page headers.
  virtual size_t CommittedMemory() const { return committed_; }

  virtual size_t MaximumCommittedMemory() const { return max_committed_; }

  // Approximate amount of physical memory committed for this space.
  virtual size_t CommittedPhysicalMemory() const = 0;

  // Returns allocated size.
  virtual size_t Size() const = 0;

#ifdef VERIFY_HEAP
  virtual void Verify(Isolate* isolate,
                      SpaceVerificationVisitor* visitor) const = 0;
#endif  // VERIFY_HEAP

 protected:
  BaseSpace(Heap* heap, AllocationSpace id) : heap_(heap), id_(id) {}

  virtual ~BaseSpace() = default;

  void AccountCommitted(size_t bytes) {
    DCHECK_GE(committed_ + bytes, committed_);
    committed_ += bytes;
    if (committed_ > max_committed_) {
      max_committed_ = committed_;
    }
  }

  void AccountUncommitted(size_t bytes) {
    DCHECK_GE(committed_, committed_ - bytes);
    committed_ -= bytes;
  }

 protected:
  Heap* heap_;
  AllocationSpace id_;

  // Keeps track of committed memory in a space.
  std::atomic<size_t> committed_{0};
  size_t max_committed_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_BASE_SPACE_H_
