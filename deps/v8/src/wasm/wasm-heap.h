// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_HEAP_H_
#define V8_WASM_HEAP_H_

#include <list>

#include "src/base/macros.h"
#include "src/vector.h"

namespace v8 {
namespace internal {
namespace wasm {

// Sorted, disjoint and non-overlapping memory ranges. A range is of the
// form [start, end). So there's no [start, end), [end, other_end),
// because that should have been reduced to [start, other_end).
using AddressRange = std::pair<Address, Address>;
class V8_EXPORT_PRIVATE DisjointAllocationPool final {
 public:
  enum ExtractionMode : bool { kAny = false, kContiguous = true };
  DisjointAllocationPool() {}

  explicit DisjointAllocationPool(Address, Address);

  DisjointAllocationPool(DisjointAllocationPool&& other) = default;
  DisjointAllocationPool& operator=(DisjointAllocationPool&& other) = default;

  // Merge the ranges of the parameter into this object. Ordering is
  // preserved. The assumption is that the passed parameter is
  // not intersecting this object - for example, it was obtained
  // from a previous Allocate{Pool}.
  void Merge(DisjointAllocationPool&&);

  // Allocate a contiguous range of size {size}. Return an empty pool on
  // failure.
  DisjointAllocationPool Allocate(size_t size) {
    return Extract(size, kContiguous);
  }

  // Allocate a sub-pool of size {size}. Return an empty pool on failure.
  DisjointAllocationPool AllocatePool(size_t size) {
    return Extract(size, kAny);
  }

  bool IsEmpty() const { return ranges_.empty(); }
  const std::list<AddressRange>& ranges() const { return ranges_; }

 private:
  // Extract out a total of {size}. By default, the return may
  // be more than one range. If kContiguous is passed, the return
  // will be one range. If the operation fails, this object is
  // unchanged, and the return {IsEmpty()}
  DisjointAllocationPool Extract(size_t size, ExtractionMode mode);

  std::list<AddressRange> ranges_;

  DISALLOW_COPY_AND_ASSIGN(DisjointAllocationPool)
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#endif
